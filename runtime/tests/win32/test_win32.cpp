// Testes da camada Win32. Uso: test_win32 [identity|offset]
//  identity: guest == host (base 0).   offset: guest = base + host (base != 0).
#include "win32_types.h"
#include "win32_abi.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <thread>
#include <vector>

static int g_pass = 0, g_fail = 0;
#define EXPECT(c) do { if (c) ++g_pass; else { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define EXPECT_EQ(a, b) do { unsigned long long _a = (unsigned long long)(a), _b = (unsigned long long)(b); \
    if (_a == _b) ++g_pass; else { ++g_fail; std::printf("  FAIL %s:%d  %s == %s  (0x%llx vs 0x%llx)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

// ---- "MachineState" simulado: registradores em array PRIVADO; o wrapper só vê 3 referências ----
class MockMachine {
public:
    enum { EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI, N };
    uint32_t& reg(int i) { return r_[i]; }
private:
    uint32_t r_[N] = {};
};
static MockMachine g_m;
static GuestCPU g_cpu{ g_m.reg(MockMachine::EAX), g_m.reg(MockMachine::EDX), g_m.reg(MockMachine::ESP) };
#define EAXV g_m.reg(MockMachine::EAX)
#define EDXV g_m.reg(MockMachine::EDX)
#define ESPV g_m.reg(MockMachine::ESP)

// ---- helpers de memória/chamada do guest ----
static constexpr uint32_t kStackBase = 0x7E000000, kStackSize = 0x100000;
static constexpr uint32_t kScratch = 0x7E010000;      // strings/buffers do teste
static constexpr uint32_t kStackTop = 0x7E0F0000;

static uint8_t* gp(uint32_t a) { return static_cast<uint8_t*>(Win32_GuestToHost(a)); }
static void gw32(uint32_t a, uint32_t v) { std::memcpy(gp(a), &v, 4); }
static uint32_t gr32(uint32_t a) { uint32_t v; std::memcpy(&v, gp(a), 4); return v; }
static uint64_t gr64(uint32_t a) { uint64_t v; std::memcpy(&v, gp(a), 8); return v; }
static void gstr(uint32_t a, const char* s) { std::strcpy(reinterpret_cast<char*>(gp(a)), s); }

static uint32_t gcall_addr(uint32_t thunk, std::initializer_list<uint32_t> args, bool cdecl_ = false) {
    const uint32_t esp0 = ESPV;
    std::vector<uint32_t> v(args);
    for (size_t i = v.size(); i-- > 0;) { ESPV -= 4; gw32(ESPV, v[i]); }
    ESPV -= 4; gw32(ESPV, 0xDEADBEEF);                                 // endereço de retorno
    const bool ok = Win32_DispatchThunk(g_cpu, thunk);
    EXPECT(ok);
    if (cdecl_) { EXPECT_EQ(ESPV, esp0 - 4u * v.size()); ESPV += 4u * static_cast<uint32_t>(v.size()); }  // caller limpa
    EXPECT_EQ(ESPV, esp0);                                              // pilha balanceada
    return EAXV;
}
static uint32_t gcall(const char* dll, const char* fn, std::initializer_list<uint32_t> args, bool cdecl_ = false) {
    const uint32_t t = Win32_ResolveImport(dll, fn);
    EXPECT(t != 0);
    return t ? gcall_addr(t, args, cdecl_) : 0xBAD0BAD0u;
}
static uint32_t LastErr() { return gcall("kernel32.dll", "GetLastError", {}); }

// ---- funções de teste (registradas em "test.dll") ----
extern "C" {
DWORD     WINAPI T_Sum3(DWORD a, DWORD b, DWORD c) { return a + b + c; }
LONG      WINAPI T_Neg(LONG a) { return -a; }
ULONGLONG WINAPI T_Mul64(ULONGLONG a, DWORD b, ULONGLONG c) { return a * b + c; }
DWORD     WINAPI T_SizeT(SIZE_T s) { return s == 0xF0000000ul ? 1u : 0u; }
DWORD     WINAPI T_Float(float f, double d, DWORD n) { return static_cast<DWORD>(f * 2.0 + d + n); }
LONGLONG  CDECL  T_Sub64(LONGLONG a, LONGLONG b) { return a - b; }
DWORD     CDECL  T_Cdecl3(DWORD a, DWORD b, DWORD c) { return a * 100 + b * 10 + c; }
DWORD     WINAPI T_StrLen(LPCSTR s) { return static_cast<DWORD>(std::strlen(s)); }
void      WINAPI T_Fill(LPVOID p, DWORD n, BYTE v) { std::memset(p, v, n); }
void      WINAPI T_StoreSeven(DWORD* out) { *out = 7; }
LPVOID    WINAPI T_RetPtr(LPVOID p) { return p; }
LPVOID    WINAPI T_RetNull() { return nullptr; }
HANDLE    WINAPI T_RetInvalid() { return INVALID_HANDLE_VALUE; }
DWORD     WINAPI T_IsInvalid(HANDLE h) { return h == INVALID_HANDLE_VALUE ? 1u : 0u; }
DWORD     WINAPI T_HandleVal(HANDLE h) { return static_cast<DWORD>(reinterpret_cast<uintptr_t>(h)); }
DWORD     WINAPI T_IsNull(LPVOID p) { return p == nullptr ? 1u : 0u; }
}
WIN32_STDCALL_EXPORT("test.dll", T_Sum3);
WIN32_STDCALL_EXPORT("test.dll", T_Neg);
WIN32_STDCALL_EXPORT("test.dll", T_Mul64);
WIN32_STDCALL_EXPORT("test.dll", T_SizeT);
WIN32_STDCALL_EXPORT("test.dll", T_Float);
WIN32_CDECL_EXPORT("test.dll", T_Sub64);
WIN32_CDECL_EXPORT("test.dll", T_Cdecl3);
WIN32_STDCALL_EXPORT("test.dll", T_StrLen);
WIN32_STDCALL_EXPORT("test.dll", T_Fill);
WIN32_STDCALL_EXPORT("test.dll", T_StoreSeven);
WIN32_STDCALL_EXPORT("test.dll", T_RetPtr);
WIN32_STDCALL_EXPORT("test.dll", T_RetNull);
WIN32_STDCALL_EXPORT("test.dll", T_RetInvalid);
WIN32_STDCALL_EXPORT("test.dll", T_IsInvalid);
WIN32_STDCALL_EXPORT("test.dll", T_HandleVal);
WIN32_STDCALL_EXPORT("test.dll", T_IsNull);

// ============================================================================
static void test_registry() {
    std::puts("[registry]");
    const uint32_t va = Win32_ResolveImport("kernel32.dll", "VirtualAlloc");
    EXPECT(va != 0);
    EXPECT(Win32_IsThunkAddress(va));
    EXPECT_EQ(Win32_ResolveImport("KERNEL32.DLL", "VirtualAlloc"), va);
    EXPECT_EQ(Win32_ResolveImport("C:\\Windows\\System32\\Kernel32.dll", "VirtualAlloc"), va);
    EXPECT_EQ(Win32_ResolveImport("kernel32.dll", "FuncaoInexistente"), 0);
    EXPECT_EQ(Win32_ResolveImport("naoexiste.dll", "VirtualAlloc"), 0);
    EXPECT(Win32_ResolveThunk(va) != nullptr);
    EXPECT(Win32_ResolveThunk(0x1234) == nullptr);
    EXPECT(Win32_ResolveThunk(va + 1) == nullptr);                  // desalinhado
    EXPECT(Win32_ResolveThunk(0x7F000000u + 16u * 900000u) == nullptr);  // faixa de thunk sem export
    EXPECT(!Win32_IsThunkAddress(0x00401000));
    // duplicado: mesmo endereço-thunk, sem consumir novo slot
    const uint32_t t1 = Win32_ResolveImport("test.dll", "T_Sum3");
    EXPECT_EQ(Win32_RegisterExport("test.dll", "T_Sum3", &GuestThunk<&T_Sum3, true>::Invoke), t1);
    // exports do kernel de arquivos e de sistema usam o MESMO registry
    for (const char* n : {"CreateFileA", "ReadFile", "WriteFile", "CloseHandle", "SetFilePointer", "GetFileSize", "GetFileAttributesA"})
        EXPECT(Win32_ResolveImport("kernel32.dll", n) != 0);
    for (const char* n : {"VirtualAlloc", "VirtualFree", "VirtualProtect", "GetTickCount", "GetTickCount64", "Sleep",
                          "QueryPerformanceCounter", "QueryPerformanceFrequency", "LoadLibraryA", "FreeLibrary",
                          "GetModuleHandleA", "GetProcAddress", "GetLastError", "SetLastError"})
        EXPECT(Win32_ResolveImport("kernel32.dll", n) != 0);
}

static void test_abi() {
    std::puts("[abi: stdcall/cdecl, pilha, 32/64 bits, ponteiros]");
    EXPECT_EQ(gcall("test.dll", "T_Sum3", {1, 2, 3}), 6);
    EXPECT_EQ(gcall("test.dll", "T_Neg", {5}), 0xFFFFFFFBu);                       // estende sinal
    // 64 bits: a(2 slots) b(1) c(2)  =>  5 slots
    EXPECT_EQ(gcall("test.dll", "T_Mul64", {3, 1, 5, 7, 0}), 22);                  // 5*(2^32+3)+7 = 5*2^32+22
    EXPECT_EQ(EDXV, 5);
    EXPECT_EQ(gcall("test.dll", "T_SizeT", {0xF0000000u}), 1);                     // SIZE_T estende zero
    float f = 1.5f; double d = 2.25; uint32_t fb, db[2];
    std::memcpy(&fb, &f, 4); std::memcpy(db, &d, 8);
    EXPECT_EQ(gcall("test.dll", "T_Float", {fb, db[0], db[1], 10}), 15);           // 3 + 2.25 + 10
    // cdecl: thunk NÃO limpa args (gcall verifica esp0-4n antes da limpeza do caller)
    EXPECT_EQ(gcall("test.dll", "T_Sub64", {1, 0, 2, 0}, true), 0xFFFFFFFFu);      // 1-2 = -1
    EXPECT_EQ(EDXV, 0xFFFFFFFFu);
    EXPECT_EQ(gcall("test.dll", "T_Cdecl3", {1, 2, 3}, true), 123);
    // ponteiros de dados são traduzidos
    gstr(kScratch, "PES2017");
    EXPECT_EQ(gcall("test.dll", "T_StrLen", {kScratch}), 7);
    std::memset(gp(kScratch + 0x100), 0, 16);
    gcall("test.dll", "T_Fill", {kScratch + 0x100, 8, 0xAB});
    EXPECT(gp(kScratch + 0x100)[7] == 0xAB && gp(kScratch + 0x100)[8] == 0);
    gw32(kScratch + 0x200, 0);
    gcall("test.dll", "T_StoreSeven", {kScratch + 0x200});
    EXPECT_EQ(gr32(kScratch + 0x200), 7);
    // ida e volta de ponteiro (em 'offset' o ponteiro host != valor guest)
    EXPECT_EQ(gcall("test.dll", "T_RetPtr", {0x12345678}), 0x12345678);
    EXPECT_EQ(gcall("test.dll", "T_RetNull", {}), 0);
    EXPECT_EQ(gcall("test.dll", "T_IsNull", {0}), 1);
    EXPECT_EQ(gcall("test.dll", "T_IsNull", {0x1000}), 0);
    // handles: opacos, NÃO traduzidos; -1 <-> 0xFFFFFFFF
    EXPECT_EQ(gcall("test.dll", "T_RetInvalid", {}), 0xFFFFFFFFu);
    EXPECT_EQ(gcall("test.dll", "T_IsInvalid", {0xFFFFFFFFu}), 1);
    EXPECT_EQ(gcall("test.dll", "T_IsInvalid", {0x100}), 0);
    EXPECT_EQ(gcall("test.dll", "T_HandleVal", {0x104}), 0x104);                    // sem somar base
}

static void test_memory() {
    std::puts("[VirtualAlloc/Free/Protect]");
    const uint32_t pg = static_cast<uint32_t>(sysconf(_SC_PAGESIZE));
    const uint32_t RW = PAGE_READWRITE;
    uint32_t p = gcall("kernel32.dll", "VirtualAlloc", {0, 0x20000, MEM_COMMIT | MEM_RESERVE, RW});
    EXPECT(p >= 0x10000000u && p < 0x7C000000u);
    EXPECT_EQ(p % 0x10000, 0);
    std::memset(gp(p), 0x5A, 0x20000);
    uint32_t p2 = gcall("kernel32.dll", "VirtualAlloc", {0, 0x1000, MEM_COMMIT | MEM_RESERVE, RW});
    EXPECT(p2 != 0 && (p2 >= p + 0x20000 || p2 + 0x10000 <= p));                    // sem sobreposição
    // decommit -> recommit => zerado
    gp(p)[pg] = 0xAB;
    EXPECT_EQ(gcall("kernel32.dll", "VirtualFree", {p + pg, pg, MEM_DECOMMIT}), 1);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualAlloc", {p + pg, pg, MEM_COMMIT, RW}), p + pg);
    EXPECT_EQ(gp(p)[pg], 0);
    EXPECT_EQ(gp(p)[0], 0x5A);                                                      // vizinho intacto
    // VirtualProtect devolve proteção anterior
    const uint32_t oldp = kScratch + 0x300;
    EXPECT_EQ(gcall("kernel32.dll", "VirtualProtect", {p, pg, PAGE_READONLY, oldp}), 1);
    EXPECT_EQ(gr32(oldp), PAGE_READWRITE);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualProtect", {p, pg, RW, oldp}), 1);
    EXPECT_EQ(gr32(oldp), PAGE_READONLY);
    // reserve-only + commit parcial
    uint32_t r = gcall("kernel32.dll", "VirtualAlloc", {0, 0x40000, MEM_RESERVE, PAGE_NOACCESS});
    EXPECT(r != 0);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualProtect", {r, pg, RW, oldp}), 0);       // não commitada
    EXPECT_EQ(LastErr(), ERROR_INVALID_ADDRESS);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualAlloc", {r + 0x1000, 0x1000, MEM_COMMIT, RW}), r + 0x1000);
    gp(r + 0x1000)[0] = 1;
    // erros
    EXPECT_EQ(gcall("kernel32.dll", "VirtualAlloc", {0, 0, MEM_COMMIT, RW}), 0);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualAlloc", {0, 0x1000, 0, RW}), 0);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualAlloc", {0, 0x1000, MEM_COMMIT, 0x3}), 0);   // proteção inválida
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualAlloc", {r, 0x10000, MEM_RESERVE, RW}), 0);  // já reservado
    EXPECT_EQ(LastErr(), ERROR_INVALID_ADDRESS);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualAlloc", {0x7B000000u, 0x1000, MEM_COMMIT, RW}), 0);  // sem reserva
    EXPECT_EQ(LastErr(), ERROR_INVALID_ADDRESS);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualAlloc", {0, 0x7C000000u, MEM_RESERVE, RW}), 0);      // maior que a arena
    EXPECT_EQ(LastErr(), ERROR_NOT_ENOUGH_MEMORY);
    uint32_t fixed = gcall("kernel32.dll", "VirtualAlloc", {0x30000000u, 0x10000, MEM_RESERVE | MEM_COMMIT, RW});
    EXPECT_EQ(fixed, 0x30000000u);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualFree", {fixed, 0, MEM_RELEASE}), 1);
    // exaustão: reserva enorme ok, segunda falha, liberar devolve espaço
    uint32_t big = gcall("kernel32.dll", "VirtualAlloc", {0, 0x60000000u, MEM_RESERVE, PAGE_NOACCESS});
    EXPECT(big != 0);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualAlloc", {0, 0x20000000u, MEM_RESERVE, PAGE_NOACCESS}), 0);
    EXPECT_EQ(LastErr(), ERROR_NOT_ENOUGH_MEMORY);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualFree", {big, 0, MEM_RELEASE}), 1);
    // free inválido
    EXPECT_EQ(gcall("kernel32.dll", "VirtualFree", {p + 0x10000, 0, MEM_RELEASE}), 0);   // não é base
    EXPECT_EQ(LastErr(), ERROR_INVALID_ADDRESS);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualFree", {p, 5, MEM_RELEASE}), 0);             // size != 0
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualFree", {p, 0, 0}), 0);                       // tipo inválido
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
    for (uint32_t b : {p, p2, r}) EXPECT_EQ(gcall("kernel32.dll", "VirtualFree", {b, 0, MEM_RELEASE}), 1);
    EXPECT_EQ(gcall("kernel32.dll", "VirtualFree", {p, 0, MEM_RELEASE}), 0);             // duplo free
    EXPECT_EQ(LastErr(), ERROR_INVALID_ADDRESS);
}

static void test_modules() {
    std::puts("[LoadLibrary/GetProcAddress/GetModuleHandle]");
    gstr(kScratch, "KERNEL32.dll");
    const uint32_t k32 = gcall("kernel32.dll", "LoadLibraryA", {kScratch});
    EXPECT(k32 != 0 && k32 != 0xFFFFFFFFu);
    gstr(kScratch, "C:\\Windows\\System32\\d3d9.dll");
    const uint32_t d3d = gcall("kernel32.dll", "LoadLibraryA", {kScratch});
    EXPECT(d3d != 0 && d3d != k32);
    gstr(kScratch, "kernel32");                                                   // sem extensão
    EXPECT_EQ(gcall("kernel32.dll", "LoadLibraryA", {kScratch}), k32);
    gstr(kScratch, "naoexiste.dll");
    EXPECT_EQ(gcall("kernel32.dll", "LoadLibraryA", {kScratch}), 0);
    EXPECT_EQ(LastErr(), ERROR_MOD_NOT_FOUND);
    EXPECT_EQ(gcall("kernel32.dll", "LoadLibraryA", {0}), 0);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(gcall("kernel32.dll", "GetModuleHandleA", {0}), 0x400000);
    gstr(kScratch, "KERNEL32.DLL");
    EXPECT_EQ(gcall("kernel32.dll", "GetModuleHandleA", {kScratch}), k32);
    EXPECT_EQ(gcall("kernel32.dll", "FreeLibrary", {k32}), 1);
    EXPECT_EQ(gcall("kernel32.dll", "FreeLibrary", {0x1234}), 0);
    EXPECT_EQ(LastErr(), ERROR_INVALID_HANDLE);

    gstr(kScratch + 0x40, "GetTickCount");
    const uint32_t pf = gcall("kernel32.dll", "GetProcAddress", {k32, kScratch + 0x40});
    EXPECT(Win32_IsThunkAddress(pf));
    EXPECT_EQ(pf, Win32_ResolveImport("kernel32.dll", "GetTickCount"));
    const uint32_t t0 = gcall_addr(pf, {});                                       // chamada dinâmica via ponteiro obtido
    EXPECT(t0 != 0);
    gstr(kScratch + 0x40, "T_Sum3");                                              // GetProcAddress em DLL "test.dll"
    gstr(kScratch + 0x80, "test.dll");
    const uint32_t tm = gcall("kernel32.dll", "LoadLibraryA", {kScratch + 0x80});
    const uint32_t ps = gcall("kernel32.dll", "GetProcAddress", {tm, kScratch + 0x40});
    EXPECT_EQ(gcall_addr(ps, {10, 20, 30}), 60);
    gstr(kScratch + 0x40, "NaoExiste");
    EXPECT_EQ(gcall("kernel32.dll", "GetProcAddress", {k32, kScratch + 0x40}), 0);
    EXPECT_EQ(LastErr(), ERROR_PROC_NOT_FOUND);
    EXPECT_EQ(gcall("kernel32.dll", "GetProcAddress", {k32, 5}), 0);              // ordinal inexistente
    EXPECT_EQ(LastErr(), ERROR_PROC_NOT_FOUND);
    EXPECT_EQ(gcall("kernel32.dll", "GetProcAddress", {0x1234, kScratch + 0x40}), 0);  // módulo inválido
    EXPECT_EQ(LastErr(), ERROR_PROC_NOT_FOUND);
    EXPECT_EQ(gcall("kernel32.dll", "GetProcAddress", {k32, 0}), 0);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
}

static void test_time() {
    std::puts("[tempo]");
    const uint32_t a = gcall("kernel32.dll", "GetTickCount", {});
    gcall("kernel32.dll", "Sleep", {30});
    const uint32_t b = gcall("kernel32.dll", "GetTickCount", {});
    EXPECT(b - a >= 25 && b - a < 1000);
    gcall("kernel32.dll", "Sleep", {0});                                          // yield
    const uint32_t lo = gcall("kernel32.dll", "GetTickCount64", {});
    const uint32_t hi = EDXV;
    EXPECT_EQ(hi, 0);
    EXPECT(lo - b < 1000);
    EXPECT_EQ(gcall("kernel32.dll", "QueryPerformanceFrequency", {kScratch + 0x400}), 1);
    EXPECT_EQ(gr64(kScratch + 0x400), 1000000000ull);
    EXPECT_EQ(gcall("kernel32.dll", "QueryPerformanceCounter", {kScratch + 0x410}), 1);
    const uint64_t c1 = gr64(kScratch + 0x410);
    gcall("kernel32.dll", "Sleep", {5});
    gcall("kernel32.dll", "QueryPerformanceCounter", {kScratch + 0x410});
    EXPECT(gr64(kScratch + 0x410) > c1);
    EXPECT_EQ(gcall("kernel32.dll", "QueryPerformanceCounter", {0}), 0);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
}

static std::string g_root, g_save;
static void host_write(const std::string& p, const std::string& data) {
    FILE* f = std::fopen(p.c_str(), "wb"); if (f) { std::fwrite(data.data(), 1, data.size(), f); std::fclose(f); }
}
static std::string host_read(const std::string& p) {
    std::string s; FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return "<ausente>";
    char b[256]; size_t n; while ((n = std::fread(b, 1, sizeof b, f)) > 0) s.append(b, n); std::fclose(f); return s;
}

static void test_paths() {
    std::puts("[traducao de caminhos]");
    char out[512];
    auto tr = [&](const char* w) { Win32_TranslatePath(w, out, sizeof out); return std::string(out); };
    EXPECT(tr("C:\\Games\\PES2017\\data.cpk") == g_root + "/data.cpk");
    EXPECT(tr("c:\\GAMES\\pes2017\\sub\\..\\x.bin") == g_root + "/x.bin");
    EXPECT(tr("data.cpk") == g_root + "/data.cpk");
    EXPECT(tr("\\\\?\\C:\\Games\\PES2017\\a.b") == g_root + "/a.b");
    EXPECT(tr("C:\\Games\\PES2017\\") == g_root);
    EXPECT(tr("C:\\Windows\\foo.dll") == g_root + "/_winfs/c/Windows/foo.dll");
    EXPECT(tr("C:\\Games\\PES2017\\..\\..\\..\\etc\\passwd") == g_root + "/_winfs/c/etc/passwd");   // não escapa
    EXPECT(tr("..\\..\\..\\etc\\passwd") == g_root + "/etc/passwd");                                  // relativo também fica dentro
    EXPECT(tr("D:\\Save\\slot1.bin") == g_save + "/slot1.bin");                                       // mount extra
    EXPECT(!Win32_TranslatePath("C:\\Games\\PES2017\\data.cpk", out, 8));                             // buffer pequeno
}

static void test_files() {
    std::puts("[arquivos]");
    std::string data; for (int i = 0; i < 1000; i++) data.push_back(static_cast<char>(i & 0xFF));
    host_write(g_root + "/Sub/Data.CPK", data);
    const uint32_t P = kScratch + 0x500, B = kScratch + 0x600, N = kScratch + 0x700, H = kScratch + 0x710, OV = kScratch + 0x720;
    auto open = [&](const char* path, uint32_t acc, uint32_t disp, uint32_t flags = FILE_ATTRIBUTE_NORMAL) {
        gstr(P, path);
        return gcall("kernel32.dll", "CreateFileA", {P, acc, 1, 0, disp, flags, 0});
    };
    const uint32_t INV = 0xFFFFFFFFu;

    uint32_t h = open("C:\\Games\\PES2017\\sub\\data.cpk", GENERIC_READ, OPEN_EXISTING);   // case diferente
    EXPECT(h != INV && h != 0 && (h % 4) == 0 && h >= 0x100);
    EXPECT_EQ(LastErr(), 0);
    gw32(H, 0xCCCCCCCC);
    EXPECT_EQ(gcall("kernel32.dll", "GetFileSize", {h, H}), 1000);
    EXPECT_EQ(gr32(H), 0);
    EXPECT_EQ(gcall("kernel32.dll", "GetFileSize", {h, 0}), 1000);
    EXPECT_EQ(gcall("kernel32.dll", "SetFilePointer", {h, 10, 0, FILE_BEGIN}), 10);
    EXPECT_EQ(gcall("kernel32.dll", "ReadFile", {h, B, 5, N, 0}), 1);
    EXPECT_EQ(gr32(N), 5);
    EXPECT(gp(B)[0] == 10 && gp(B)[4] == 14);
    EXPECT_EQ(gcall("kernel32.dll", "SetFilePointer", {h, 0, 0, FILE_CURRENT}), 15);        // ponteiro avançou 5
    EXPECT_EQ(gcall("kernel32.dll", "SetFilePointer", {h, static_cast<uint32_t>(-4), 0, FILE_END}), 996);
    gw32(H, 0);
    EXPECT_EQ(gcall("kernel32.dll", "SetFilePointer", {h, 100, H, FILE_BEGIN}), 100);       // com High
    EXPECT_EQ(gr32(H), 0);
    gcall("kernel32.dll", "SetFilePointer", {h, 996, 0, FILE_BEGIN});
    EXPECT_EQ(gcall("kernel32.dll", "ReadFile", {h, B, 500, N, 0}), 1);                     // EOF parcial
    EXPECT_EQ(gr32(N), 4);
    EXPECT_EQ(gcall("kernel32.dll", "ReadFile", {h, B, 10, N, 0}), 1);                      // no EOF: 0 bytes, TRUE
    EXPECT_EQ(gr32(N), 0);
    // OVERLAPPED: pread em offset 20, não move o ponteiro
    gcall("kernel32.dll", "SetFilePointer", {h, 500, 0, FILE_BEGIN});
    std::memset(gp(OV), 0, 20); gw32(OV + 8, 20);
    EXPECT_EQ(gcall("kernel32.dll", "ReadFile", {h, B, 3, N, OV}), 1);
    EXPECT(gp(B)[0] == 20 && gp(B)[2] == 22);
    EXPECT_EQ(gr32(OV + 4), 3);                                                             // InternalHigh
    EXPECT_EQ(gcall("kernel32.dll", "ReadFile", {h, B, 1, N, 0}), 1);
    EXPECT_EQ(gp(B)[0], 500 & 0xFF);
    // seek inválido
    EXPECT_EQ(gcall("kernel32.dll", "SetFilePointer", {h, static_cast<uint32_t>(-5), 0, FILE_BEGIN}), INV);
    EXPECT_EQ(LastErr(), 131);                                                              // ERROR_NEGATIVE_SEEK
    EXPECT_EQ(gcall("kernel32.dll", "SetFilePointer", {h, 0, 0, 9}), INV);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
    // handle somente-leitura: WriteFile => ACCESS_DENIED
    EXPECT_EQ(gcall("kernel32.dll", "WriteFile", {h, B, 1, N, 0}), 0);
    EXPECT_EQ(LastErr(), ERROR_ACCESS_DENIED);
    EXPECT_EQ(gcall("kernel32.dll", "CloseHandle", {h}), 1);
    EXPECT_EQ(gcall("kernel32.dll", "CloseHandle", {h}), 0);                                // duplo close
    EXPECT_EQ(LastErr(), ERROR_INVALID_HANDLE);

    // handles inválidos
    for (uint32_t bad : {0u, INV, 0x104u, 0x7FFFFFF0u}) {
        EXPECT_EQ(gcall("kernel32.dll", "CloseHandle", {bad}), 0);
        EXPECT_EQ(LastErr(), ERROR_INVALID_HANDLE);
        EXPECT_EQ(gcall("kernel32.dll", "ReadFile", {bad, B, 1, N, 0}), 0);
        EXPECT_EQ(LastErr(), ERROR_INVALID_HANDLE);
        EXPECT_EQ(gcall("kernel32.dll", "WriteFile", {bad, B, 1, N, 0}), 0);
        EXPECT_EQ(LastErr(), ERROR_INVALID_HANDLE);
        EXPECT_EQ(gcall("kernel32.dll", "GetFileSize", {bad, 0}), INV);
        EXPECT_EQ(LastErr(), ERROR_INVALID_HANDLE);
        EXPECT_EQ(gcall("kernel32.dll", "SetFilePointer", {bad, 0, 0, FILE_BEGIN}), INV);
        EXPECT_EQ(LastErr(), ERROR_INVALID_HANDLE);
    }
    // erros de abertura
    EXPECT_EQ(open("C:\\Games\\PES2017\\nao_existe.bin", GENERIC_READ, OPEN_EXISTING), INV);
    EXPECT_EQ(LastErr(), ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(open("C:\\Games\\PES2017\\nodir\\x.bin", GENERIC_READ, OPEN_EXISTING), INV);
    EXPECT_EQ(LastErr(), ERROR_PATH_NOT_FOUND);
    EXPECT_EQ(open("C:\\Games\\PES2017\\Sub", GENERIC_READ, OPEN_EXISTING), INV);          // diretório sem BACKUP_SEMANTICS
    EXPECT_EQ(LastErr(), ERROR_ACCESS_DENIED);
    EXPECT_EQ(open("C:\\Games\\PES2017\\sub\\data.cpk", GENERIC_READ, 9), INV);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(open("C:\\Games\\PES2017\\sub\\data.cpk", GENERIC_READ, TRUNCATE_EXISTING), INV);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(gcall("kernel32.dll", "CreateFileA", {0, GENERIC_READ, 1, 0, OPEN_EXISTING, 0, 0}), INV);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);

    // criação / escrita / disposições
    uint32_t w = open("C:\\Games\\PES2017\\new.bin", GENERIC_WRITE, CREATE_NEW);
    EXPECT(w != INV);
    gstr(B, "HELLO");
    EXPECT_EQ(gcall("kernel32.dll", "WriteFile", {w, B, 5, N, 0}), 1);
    EXPECT_EQ(gr32(N), 5);
    EXPECT_EQ(gcall("kernel32.dll", "ReadFile", {w, B, 1, N, 0}), 0);                        // só-escrita
    EXPECT_EQ(LastErr(), ERROR_ACCESS_DENIED);
    EXPECT_EQ(gcall("kernel32.dll", "CloseHandle", {w}), 1);
    EXPECT(host_read(g_root + "/new.bin") == "HELLO");
    EXPECT_EQ(open("C:\\Games\\PES2017\\new.bin", GENERIC_WRITE, CREATE_NEW), INV);
    EXPECT_EQ(LastErr(), ERROR_FILE_EXISTS);
    w = open("C:\\Games\\PES2017\\NEW.BIN", GENERIC_READ | GENERIC_WRITE, CREATE_ALWAYS);   // existente (case) => trunca
    EXPECT(w != INV);
    EXPECT_EQ(LastErr(), ERROR_ALREADY_EXISTS);
    EXPECT_EQ(gcall("kernel32.dll", "GetFileSize", {w, 0}), 0);
    gcall("kernel32.dll", "CloseHandle", {w});
    w = open("C:\\Games\\PES2017\\open_always.bin", GENERIC_WRITE, OPEN_ALWAYS);
    EXPECT(w != INV);
    EXPECT_EQ(LastErr(), 0);
    gcall("kernel32.dll", "CloseHandle", {w});
    w = open("C:\\Games\\PES2017\\SUB\\Criado.bin", GENERIC_WRITE, CREATE_ALWAYS);           // pasta pai com case diferente
    EXPECT(w != INV);
    gcall("kernel32.dll", "CloseHandle", {w});
    EXPECT(host_read(g_root + "/Sub/Criado.bin") == "");
    w = open("D:\\Save\\slot1.bin", GENERIC_WRITE, CREATE_ALWAYS);                           // mount extra
    EXPECT(w != INV);
    gcall("kernel32.dll", "CloseHandle", {w});
    EXPECT(host_read(g_save + "/slot1.bin") == "");

    // GetFileAttributesA
    gstr(P, "C:\\Games\\PES2017\\Sub");
    EXPECT_EQ(gcall("kernel32.dll", "GetFileAttributesA", {P}) & FILE_ATTRIBUTE_DIRECTORY, FILE_ATTRIBUTE_DIRECTORY);
    gstr(P, "C:\\Games\\PES2017\\sub\\data.cpk");
    const uint32_t at = gcall("kernel32.dll", "GetFileAttributesA", {P});
    EXPECT(at != INV && !(at & FILE_ATTRIBUTE_DIRECTORY));
    gstr(P, "C:\\Games\\PES2017\\nada.bin");
    EXPECT_EQ(gcall("kernel32.dll", "GetFileAttributesA", {P}), INV);
    EXPECT_EQ(LastErr(), ERROR_FILE_NOT_FOUND);
    gstr(P, "C:\\Games\\PES2017\\nodir\\nada.bin");
    EXPECT_EQ(gcall("kernel32.dll", "GetFileAttributesA", {P}), INV);
    EXPECT_EQ(LastErr(), ERROR_PATH_NOT_FOUND);
    EXPECT_EQ(gcall("kernel32.dll", "GetFileAttributesA", {0}), INV);
    EXPECT_EQ(LastErr(), ERROR_INVALID_PARAMETER);

    // IDs únicos e liberação em massa
    std::vector<uint32_t> hs;
    for (int i = 0; i < 50; i++) hs.push_back(open("C:\\Games\\PES2017\\sub\\data.cpk", GENERIC_READ, OPEN_EXISTING));
    bool uniq = true; for (size_t i = 0; i < hs.size(); i++) for (size_t j = i + 1; j < hs.size(); j++) uniq &= (hs[i] != hs[j]);
    EXPECT(uniq);
    for (uint32_t x : hs) EXPECT_EQ(gcall("kernel32.dll", "CloseHandle", {x}), 1);
}

static void test_lasterror_threads() {
    std::puts("[GetLastError por thread]");
    gcall("kernel32.dll", "SetLastError", {5});
    DWORD other = 99;
    std::thread t([&] { other = GetLastError(); SetLastError(9); });
    t.join();
    EXPECT_EQ(other, 0);
    EXPECT_EQ(LastErr(), 5);
}

int main(int argc, char** argv) {
    const bool offset = argc > 1 && std::strcmp(argv[1], "offset") == 0;
    std::printf("== modo: %s ==\n", offset ? "offset (guest = base + host)" : "identity (guest == host)");

    if (offset) {   // simula o MachineState reservando 2GB de espaço guest em endereço arbitrário
        void* base = mmap(nullptr, 0x80000000ull, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (base == MAP_FAILED) { std::puts("falha ao reservar espaco guest"); return 2; }
        Win32_ConfigureGuestMemory(base);
        EXPECT(g_win32GuestBase != 0);
        void* st = mmap(static_cast<uint8_t*>(base) + kStackBase, kStackSize, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (st == MAP_FAILED) { std::puts("falha ao mapear pilha"); return 2; }
    } else {
        void* st = mmap(reinterpret_cast<void*>(kStackBase), kStackSize, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (st != reinterpret_cast<void*>(kStackBase)) { std::puts("falha ao mapear pilha"); return 2; }
    }
    ESPV = kStackTop;

    char tmpl[] = "/tmp/pes_win32_XXXXXX";
    if (!mkdtemp(tmpl)) return 2;
    g_root = tmpl; g_save = g_root + "_save";
    mkdir((g_root + "/Sub").c_str(), 0755);
    mkdir(g_save.c_str(), 0755);
    Win32_SetAndroidRoot(g_root.c_str());
    Win32_AddPathMount("D:\\Save", g_save.c_str());

    test_registry();
    test_abi();
    test_memory();
    test_modules();
    test_time();
    test_paths();
    test_files();
    test_lasterror_threads();

    EXPECT_EQ(ESPV, kStackTop);   // pilha do guest sem vazamento ao final
    std::printf("== resultado: %d passaram, %d falharam ==\n", g_pass, g_fail);
    std::string cmd = "rm -rf '" + g_root + "' '" + g_save + "'";
    if (std::system(cmd.c_str()) != 0) {}
    return g_fail ? 1 : 0;
}
