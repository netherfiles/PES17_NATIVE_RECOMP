// ============================================================================
// win32_stubs.cpp - Memória virtual, tempo e carregamento dinâmico (kernel32)
// ============================================================================
#include "win32_types.h"

#include <sys/mman.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <errno.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

// ============================================================================
// Log
// ============================================================================
extern "C" void Win32_Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
#ifdef __ANDROID__
    __android_log_vprint(ANDROID_LOG_INFO, "PES17-Win32", fmt, ap);
#else
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
#endif
    va_end(ap);
}

// ============================================================================
// GetLastError / SetLastError (por thread)
// ============================================================================
static thread_local DWORD t_lastError = 0;

WIN32_API DWORD WINAPI GetLastError(void)            { return t_lastError; }
WIN32_API void  WINAPI SetLastError(DWORD dwErrCode) { t_lastError = dwErrCode; }

// ============================================================================
// Memória virtual
//
// Uma única arena PROT_NONE é reservada em endereço fixo < 4GB. VirtualAlloc
// sub-aloca regiões (granularidade 64KB, como no Windows) e faz commit/decommit
// por página com mprotect/madvise. Resultado: ponteiros do guest == ponteiros host.
// ============================================================================
namespace {

constexpr uintptr_t kArenaBase        = 0x10000000u;
constexpr size_t    kArenaSize        = 0x6C000000u;   // 0x10000000 .. 0x7C000000 (~1.7GB)
constexpr size_t    kAllocGranularity = 0x10000u;      // 64KB

struct Region {
    uintptr_t             base = 0;
    size_t                size = 0;
    std::vector<uint32_t> pageProt;   // por página do host; 0 = não commitada, senão PAGE_*
};

std::mutex                g_vmMutex;
std::map<uintptr_t, Region> g_regions;   // chave = base
bool                      g_arenaReady = false;

size_t HostPage() {
    static const size_t p = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    return p;
}
uintptr_t AlignUp(uintptr_t v, size_t a)   { return (v + a - 1) & ~(static_cast<uintptr_t>(a) - 1); }
uintptr_t AlignDown(uintptr_t v, size_t a) { return v & ~(static_cast<uintptr_t>(a) - 1); }

// PAGE_* -> PROT_*. Retorna -1 se inválido.
// PROT_EXEC é descartado de propósito: o código é recompilado estaticamente, o guest
// nunca executa bytes da própria memória, e W+X anônimo é negado pelo SELinux
// (untrusted_app) no Android moderno.
int ToProt(DWORD p) {
    switch (p & 0xFFu) {
        case PAGE_NOACCESS:           return PROT_NONE;
        case PAGE_READONLY:
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:       return PROT_READ;
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:  return PROT_READ | PROT_WRITE;
        default:                      return -1;
    }
}

bool EnsureArena() {   // g_vmMutex já travado
    if (g_arenaReady) return true;
    void* p = mmap(reinterpret_cast<void*>(kArenaBase), kArenaSize, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED) {
        Win32_Log("VirtualAlloc: falha ao reservar arena @0x%zx (%s)", (size_t)kArenaBase, strerror(errno));
        return false;
    }
    if (reinterpret_cast<uintptr_t>(p) != kArenaBase) {   // kernel antigo ignorou FIXED_NOREPLACE
        munmap(p, kArenaSize);
        Win32_Log("VirtualAlloc: arena caiu em %p (esperado 0x%zx)", p, (size_t)kArenaBase);
        return false;
    }
    g_arenaReady = true;
    Win32_Log("VirtualAlloc: arena guest 0x%zx..0x%zx pronta", (size_t)kArenaBase, (size_t)(kArenaBase + kArenaSize));
    return true;
}

// Primeiro intervalo livre de 'size' bytes (first-fit). 0 se não houver.
uintptr_t FindFree(size_t size) {
    uintptr_t cursor = kArenaBase;
    const uintptr_t end = kArenaBase + kArenaSize;
    for (const auto& kv : g_regions) {
        if (kv.first - cursor >= size) return cursor;
        cursor = kv.first + kv.second.size;
    }
    return (end - cursor >= size) ? cursor : 0;
}

bool RangeFree(uintptr_t base, size_t size) {
    if (base < kArenaBase || base + size > kArenaBase + kArenaSize || base + size < base) return false;
    auto next = g_regions.lower_bound(base);
    if (next != g_regions.end() && next->first < base + size) return false;
    if (next != g_regions.begin()) {
        auto prev = std::prev(next);
        if (prev->first + prev->second.size > base) return false;
    }
    return true;
}

Region* FindRegion(uintptr_t a, size_t len) {
    auto it = g_regions.upper_bound(a);
    if (it == g_regions.begin()) return nullptr;
    --it;
    Region& r = it->second;
    if (a >= r.base && a + len >= a && a + len <= r.base + r.size) return &r;
    return nullptr;
}

bool CommitRange(Region& r, uintptr_t a, size_t len, DWORD winProt) {
    const size_t hp = HostPage();
    const uintptr_t s = AlignDown(a, hp);
    const uintptr_t e = AlignUp(a + len, hp);
    if (mprotect(reinterpret_cast<void*>(s), e - s, ToProt(winProt)) != 0) return false;
    for (uintptr_t p = s; p < e; p += hp) r.pageProt[(p - r.base) / hp] = (winProt & 0xFFu);
    return true;
}

void DecommitRange(Region& r, uintptr_t a, size_t len) {
    const size_t hp = HostPage();
    const uintptr_t s = AlignDown(a, hp);
    const uintptr_t e = AlignUp(a + len, hp);
    madvise(reinterpret_cast<void*>(s), e - s, MADV_DONTNEED);   // zera na próxima commit
    mprotect(reinterpret_cast<void*>(s), e - s, PROT_NONE);
    for (uintptr_t p = s; p < e; p += hp) r.pageProt[(p - r.base) / hp] = 0;
}

} // namespace

WIN32_API LPVOID WINAPI VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    bool reserve = (flAllocationType & MEM_RESERVE) != 0;
    const bool commit = (flAllocationType & MEM_COMMIT) != 0;

    if (flAllocationType & MEM_RESET) return lpAddress;   // dica de "conteúdo descartável": no-op
    if (!reserve && !commit)                           { SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    if (dwSize == 0)                                   { SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    if (dwSize > kArenaSize)                           { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
    if (commit && ToProt(flProtect) < 0)               { SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    if (!lpAddress) reserve = true;                    // MEM_COMMIT sozinho com NULL => reserve+commit

    const uintptr_t addr = reinterpret_cast<uintptr_t>(lpAddress);
    std::lock_guard<std::mutex> lk(g_vmMutex);
    if (!EnsureArena()) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
    const size_t hp = HostPage();

    if (reserve) {
        uintptr_t base;
        size_t sz;
        if (!addr) {
            sz = AlignUp(dwSize, kAllocGranularity);
            base = FindFree(sz);
            if (!base) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
        } else {
            base = AlignDown(addr, kAllocGranularity);
            sz = AlignUp(addr + dwSize - base, kAllocGranularity);
            if (!RangeFree(base, sz)) { SetLastError(ERROR_INVALID_ADDRESS); return nullptr; }
        }
        Region& r = g_regions[base];
        r.base = base;
        r.size = sz;
        r.pageProt.assign(sz / hp, 0);
        if (commit && !CommitRange(r, addr ? addr : base, dwSize, flProtect)) {
            g_regions.erase(base);
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return nullptr;
        }
        return reinterpret_cast<LPVOID>(base);
    }

    // Somente commit dentro de uma reserva existente
    Region* r = FindRegion(addr, dwSize);
    if (!r) { SetLastError(ERROR_INVALID_ADDRESS); return nullptr; }
    if (!CommitRange(*r, addr, dwSize, flProtect)) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
    return reinterpret_cast<LPVOID>(AlignDown(addr, 4096));
}

WIN32_API BOOL WINAPI VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    const uintptr_t addr = reinterpret_cast<uintptr_t>(lpAddress);
    std::lock_guard<std::mutex> lk(g_vmMutex);

    if (dwFreeType & MEM_RELEASE) {
        if (!addr || dwSize != 0) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
        auto it = g_regions.find(addr);
        if (it == g_regions.end()) { SetLastError(ERROR_INVALID_ADDRESS); return FALSE; }
        DecommitRange(it->second, it->second.base, it->second.size);   // arena continua reservada
        g_regions.erase(it);
        return TRUE;
    }

    if (dwFreeType & MEM_DECOMMIT) {
        if (!addr) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
        if (dwSize == 0) {   // decommit da região inteira (addr deve ser a base)
            auto it = g_regions.find(addr);
            if (it == g_regions.end()) { SetLastError(ERROR_INVALID_ADDRESS); return FALSE; }
            DecommitRange(it->second, it->second.base, it->second.size);
            return TRUE;
        }
        Region* r = FindRegion(addr, dwSize);
        if (!r) { SetLastError(ERROR_INVALID_ADDRESS); return FALSE; }
        DecommitRange(*r, addr, dwSize);
        return TRUE;
    }

    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}

WIN32_API BOOL WINAPI VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, LPDWORD lpflOldProtect) {
    const int prot = ToProt(flNewProtect);
    if (prot < 0 || !lpflOldProtect || dwSize == 0) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }

    const uintptr_t addr = reinterpret_cast<uintptr_t>(lpAddress);
    std::lock_guard<std::mutex> lk(g_vmMutex);
    Region* r = FindRegion(addr, dwSize);
    if (!r) { SetLastError(ERROR_INVALID_ADDRESS); return FALSE; }

    const size_t hp = HostPage();
    const uintptr_t s = AlignDown(addr, hp);
    const uintptr_t e = AlignUp(addr + dwSize, hp);
    for (uintptr_t p = s; p < e; p += hp)
        if (r->pageProt[(p - r->base) / hp] == 0) { SetLastError(ERROR_INVALID_ADDRESS); return FALSE; }

    const DWORD old = r->pageProt[(s - r->base) / hp];
    if (mprotect(reinterpret_cast<void*>(s), e - s, prot) != 0) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    for (uintptr_t p = s; p < e; p += hp) r->pageProt[(p - r->base) / hp] = (flNewProtect & 0xFFu);
    *lpflOldProtect = old;
    return TRUE;
}

// ============================================================================
// Tempo
// CLOCK_MONOTONIC: não "salta" ao suspender o aparelho (melhor para loop de jogo).
// ============================================================================
static uint64_t MonotonicNs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
}

WIN32_API ULONGLONG WINAPI GetTickCount64(void) { return MonotonicNs() / 1000000ull; }
WIN32_API DWORD     WINAPI GetTickCount(void)   { return static_cast<DWORD>(GetTickCount64()); }

WIN32_API void WINAPI Sleep(DWORD dwMilliseconds) {
    if (dwMilliseconds == 0) { sched_yield(); return; }
    if (dwMilliseconds == INFINITE) { for (;;) sleep(3600); }
    timespec req;
    req.tv_sec  = static_cast<time_t>(dwMilliseconds / 1000u);
    req.tv_nsec = static_cast<long>(dwMilliseconds % 1000u) * 1000000L;
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {}
}

WIN32_API BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
    if (!lpPerformanceCount) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    lpPerformanceCount->QuadPart = static_cast<LONGLONG>(MonotonicNs());
    return TRUE;
}

WIN32_API BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency) {
    if (!lpFrequency) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    lpFrequency->QuadPart = 1000000000ll;   // contador em nanossegundos
    return TRUE;
}

// ============================================================================
// LoadLibraryA / GetProcAddress
//
// Cada função host registrada vira um "thunk": endereço falso de 32 bits em
// [kThunkBase, kThunkLimit). O guest guarda esse valor como ponteiro de função;
// o dispatcher de chamadas indiretas do recompilador chama Win32_ResolveThunk().
// HMODULE = ID falso de 32 bits (nunca ponteiro do host).
// ============================================================================
namespace {

constexpr uint32_t kThunkBase        = 0x7F000000u;
constexpr uint32_t kThunkLimit       = 0x80000000u;
constexpr uint32_t kThunkStride      = 16u;
constexpr uint32_t kModuleBase       = 0x7D000000u;   // 256 módulos, passo de 64KB
constexpr uint32_t kModuleStride     = 0x10000u;
constexpr uint32_t kModuleMax        = 256u;
constexpr uint32_t kMainModuleHandle = 0x00400000u;   // imagem base típica do .exe

struct Registry {
    std::mutex                              m;
    std::vector<void*>                      fns;        // índice = (thunk - kThunkBase) / stride
    std::unordered_map<std::string, uint32_t> byKey;    // "dll!func" -> índice
    std::vector<std::string>                modules;    // índice -> nome normalizado
    std::unordered_map<std::string, uint32_t> moduleIdx;

    Registry() {
        // DLLs de sistema "conhecidas": LoadLibraryA tem sucesso mesmo sem exports ainda.
        static const char* const known[] = {
            "kernel32.dll", "user32.dll", "d3d9.dll", "gdi32.dll", "advapi32.dll",
            "winmm.dll", "dinput8.dll", "dsound.dll", "xinput1_3.dll", "xinput1_4.dll",
            "ws2_32.dll", "shell32.dll", "ole32.dll", "msvcrt.dll", "ntdll.dll",
            "version.dll", "imm32.dll", "d3dx9_43.dll"
        };
        for (const char* k : known) AddModule(k);
    }

    uint32_t AddModule(const std::string& name) {   // m travado (ou construtor)
        auto it = moduleIdx.find(name);
        if (it != moduleIdx.end()) return it->second;
        if (modules.size() >= kModuleMax) return UINT32_MAX;
        const uint32_t idx = static_cast<uint32_t>(modules.size());
        modules.push_back(name);
        moduleIdx[name] = idx;
        return idx;
    }
};

Registry& R() { static Registry r; return r; }

// "C:\Windows\System32\D3D9.DLL" -> "d3d9.dll"
std::string NormalizeDll(const char* path) {
    std::string s(path);
    const size_t slash = s.find_last_of("/\\");
    if (slash != std::string::npos) s.erase(0, slash + 1);
    for (char& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (s.find('.') == std::string::npos) s += ".dll";
    return s;
}

HMODULE ModuleHandle(uint32_t idx) {
    return reinterpret_cast<HMODULE>(static_cast<uintptr_t>(kModuleBase + idx * kModuleStride));
}

bool DecodeModule(HMODULE h, uint32_t& idx, const Registry& reg) {   // m travado
    const uintptr_t v = reinterpret_cast<uintptr_t>(h);
    if (v < kModuleBase || v >= kModuleBase + kModuleMax * kModuleStride) return false;
    if ((v - kModuleBase) % kModuleStride) return false;
    idx = static_cast<uint32_t>((v - kModuleBase) / kModuleStride);
    return idx < reg.modules.size();
}

} // namespace

extern "C" uint32_t Win32_RegisterExport(const char* dll, const char* name, void* hostFn) {
    Registry& reg = R();
    std::lock_guard<std::mutex> lk(reg.m);
    const std::string d = NormalizeDll(dll);
    reg.AddModule(d);
    const std::string key = d + "!" + name;

    auto it = reg.byKey.find(key);
    if (it != reg.byKey.end()) {
        Win32_Log("Export duplicado %s (sobrescrito)", key.c_str());
        reg.fns[it->second] = hostFn;
        return kThunkBase + it->second * kThunkStride;
    }
    const uint32_t idx = static_cast<uint32_t>(reg.fns.size());
    if (kThunkBase + idx * kThunkStride >= kThunkLimit) {
        Win32_Log("Tabela de thunks cheia");
        return 0;
    }
    reg.fns.push_back(hostFn);
    reg.byKey[key] = idx;
    return kThunkBase + idx * kThunkStride;
}

extern "C" uint32_t Win32_ResolveImport(const char* dll, const char* name) {
    Registry& reg = R();
    std::lock_guard<std::mutex> lk(reg.m);
    auto it = reg.byKey.find(NormalizeDll(dll) + "!" + name);
    if (it == reg.byKey.end()) {
        Win32_Log("Import NAO IMPLEMENTADO: %s!%s", NormalizeDll(dll).c_str(), name);
        return 0;
    }
    return kThunkBase + it->second * kThunkStride;
}

extern "C" bool Win32_IsThunkAddress(uint32_t a) {
    return a >= kThunkBase && a < kThunkLimit && ((a - kThunkBase) % kThunkStride) == 0;
}

extern "C" void* Win32_ResolveThunk(uint32_t a) {
    if (!Win32_IsThunkAddress(a)) return nullptr;
    Registry& reg = R();
    std::lock_guard<std::mutex> lk(reg.m);
    const uint32_t idx = (a - kThunkBase) / kThunkStride;
    return idx < reg.fns.size() ? reg.fns[idx] : nullptr;
}

WIN32_API HMODULE WINAPI LoadLibraryA(LPCSTR lpLibFileName) {
    if (!lpLibFileName || !*lpLibFileName) { SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    Registry& reg = R();
    std::lock_guard<std::mutex> lk(reg.m);
    const std::string name = NormalizeDll(lpLibFileName);
    auto it = reg.moduleIdx.find(name);
    if (it == reg.moduleIdx.end()) {
        Win32_Log("LoadLibraryA('%s'): módulo desconhecido", name.c_str());
        SetLastError(ERROR_MOD_NOT_FOUND);
        return nullptr;
    }
    return ModuleHandle(it->second);
}

WIN32_API BOOL WINAPI FreeLibrary(HMODULE hLibModule) {
    Registry& reg = R();
    std::lock_guard<std::mutex> lk(reg.m);
    uint32_t idx;
    if (!DecodeModule(hLibModule, idx, reg)) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    return TRUE;   // módulos são permanentes
}

WIN32_API HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName) {
    if (!lpModuleName) return reinterpret_cast<HMODULE>(static_cast<uintptr_t>(kMainModuleHandle));
    Registry& reg = R();
    std::lock_guard<std::mutex> lk(reg.m);
    auto it = reg.moduleIdx.find(NormalizeDll(lpModuleName));
    if (it == reg.moduleIdx.end()) { SetLastError(ERROR_MOD_NOT_FOUND); return nullptr; }
    return ModuleHandle(it->second);
}

WIN32_API FARPROC WINAPI GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    if (!lpProcName) { SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    Registry& reg = R();
    std::lock_guard<std::mutex> lk(reg.m);

    uint32_t midx;
    if (!DecodeModule(hModule, midx, reg)) { SetLastError(ERROR_PROC_NOT_FOUND); return nullptr; }

    std::string fn;
    if (reinterpret_cast<uintptr_t>(lpProcName) <= 0xFFFFu)           // import por ordinal
        fn = "#" + std::to_string(reinterpret_cast<uintptr_t>(lpProcName));
    else
        fn = lpProcName;

    const std::string key = reg.modules[midx] + "!" + fn;
    auto it = reg.byKey.find(key);
    if (it == reg.byKey.end()) {
        Win32_Log("GetProcAddress MISS: %s", key.c_str());   // => stub faltando
        SetLastError(ERROR_PROC_NOT_FOUND);
        return nullptr;
    }
    return reinterpret_cast<FARPROC>(static_cast<uintptr_t>(kThunkBase + it->second * kThunkStride));
}

// ============================================================================
// Registro dos exports deste arquivo
// ============================================================================
WIN32_EXPORT("kernel32.dll", GetLastError);
WIN32_EXPORT("kernel32.dll", SetLastError);
WIN32_EXPORT("kernel32.dll", VirtualAlloc);
WIN32_EXPORT("kernel32.dll", VirtualFree);
WIN32_EXPORT("kernel32.dll", VirtualProtect);
WIN32_EXPORT("kernel32.dll", GetTickCount);
WIN32_EXPORT("kernel32.dll", GetTickCount64);
WIN32_EXPORT("kernel32.dll", Sleep);
WIN32_EXPORT("kernel32.dll", QueryPerformanceCounter);
WIN32_EXPORT("kernel32.dll", QueryPerformanceFrequency);
WIN32_EXPORT("kernel32.dll", LoadLibraryA);
WIN32_EXPORT("kernel32.dll", FreeLibrary);
WIN32_EXPORT("kernel32.dll", GetModuleHandleA);
WIN32_EXPORT("kernel32.dll", GetProcAddress);
