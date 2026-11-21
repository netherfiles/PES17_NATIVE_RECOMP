#pragma once
// ============================================================================
// win32_abi.h - Ponte ABI: pilha do guest x86 (stdcall/cdecl) -> funções C++ host
//
// Um "thunk" é uma função  void(GuestCPU&)  gerada por template a partir da
// ASSINATURA da função stub. Ele:
//   1. lê os argumentos da pilha do guest (esp+4, esp+8, ...),
//   2. converte cada um para o tipo host correto (ponteiro, int, 64 bits, float),
//   3. chama o stub,
//   4. escreve o retorno em eax (ou edx:eax),
//   5. emula o 'ret' / 'ret N' ajustando esp.
//
// Pré-requisito do recompilador: o estado do guest vive numa struct de CPU com
// registradores de 32 bits e a PILHA É REAL (pushes escrevem na memória do guest,
// esp é mantido). Sem isso não há de onde ler os argumentos.
// ============================================================================

#include "win32_types.h"

#include <array>
#include <bit>
#include <cstring>
#include <type_traits>
#include <utility>

// 1 = o 'call' do guest empurra o endereço de retorno antes de entrar no thunk
//     (args começam em esp+4).  0 = o recompilador usa call C++ puro, sem push
//     de retorno (args começam em esp+0).
#ifndef WIN32_GUEST_CALL_PUSHES_RETADDR
#define WIN32_GUEST_CALL_PUSHES_RETADDR 1
#endif

// Se o seu motor já tem uma struct de contexto, defina WIN32_ABI_CUSTOM_CPU e faça
// `using GuestCPU = SeuContexto;` ANTES de incluir este header. Campos exigidos:
// uint32_t eax, edx, esp.
#ifndef WIN32_ABI_CUSTOM_CPU
struct GuestCPU {
    uint32_t eax = 0, ecx = 0, edx = 0, ebx = 0;
    uint32_t esp = 0, ebp = 0, esi = 0, edi = 0;
};
#endif

typedef void (*GuestThunkFn)(GuestCPU&);

namespace win32abi {

// Leitura de 32 bits da memória do guest (arena idêntica => endereço == ponteiro host)
inline uint32_t ReadU32(uint32_t guestAddr) {
    uint32_t v;
    std::memcpy(&v, reinterpret_cast<const void*>(static_cast<uintptr_t>(guestAddr)), sizeof(v));
    return v;
}

// Tipos de 64 bits ocupam 2 slots de pilha (low, high). 'long long' é de propósito
// distinto de size_t/uintptr_t (que são 'unsigned long' no LP64 e ocupam 1 slot).
template <typename T>
inline constexpr bool kIsWide = std::is_same_v<T, long long> ||
                                std::is_same_v<T, unsigned long long> ||
                                std::is_same_v<T, double>;

template <typename T>
inline constexpr uint32_t kSlots = kIsWide<T> ? 2u : 1u;

// Offsets (em slots de 4 bytes) de cada argumento; o último elemento = total de slots.
template <typename... A>
constexpr std::array<uint32_t, sizeof...(A) + 1> MakeOffsets() {
    std::array<uint32_t, sizeof...(A) + 1> a{};
    const uint32_t s[sizeof...(A) + 1] = { kSlots<A>..., 0u };
    uint32_t o = 0;
    for (size_t i = 0; i < sizeof...(A); ++i) { a[i] = o; o += s[i]; }
    a[sizeof...(A)] = o;
    return a;
}

// Slot(s) da pilha -> tipo host
template <typename T>
inline T FromStack(uint32_t addr) {
    using U = std::remove_cv_t<T>;
    static_assert(!std::is_class_v<U> && !std::is_union_v<U>, "struct por valor nao suportada");
    if constexpr (std::is_same_v<U, double>) {
        const uint64_t v = ReadU32(addr) | (static_cast<uint64_t>(ReadU32(addr + 4)) << 32);
        return std::bit_cast<double>(v);
    } else if constexpr (std::is_same_v<U, float>) {
        return std::bit_cast<float>(ReadU32(addr));
    } else if constexpr (std::is_pointer_v<U>) {
        const uint32_t v = ReadU32(addr);
        // 0xFFFFFFFF = INVALID_HANDLE_VALUE do guest -> (HANDLE)-1 do host (como o Win64 faz)
        if (v == 0xFFFFFFFFu) return reinterpret_cast<U>(static_cast<intptr_t>(-1));
        return reinterpret_cast<U>(static_cast<uintptr_t>(v));
    } else if constexpr (kIsWide<U>) {
        const uint64_t v = ReadU32(addr) | (static_cast<uint64_t>(ReadU32(addr + 4)) << 32);
        return static_cast<U>(v);
    } else if constexpr (std::is_integral_v<U> || std::is_enum_v<U>) {
        if constexpr (std::is_signed_v<U>) return static_cast<U>(static_cast<int32_t>(ReadU32(addr))); // estende sinal
        else                               return static_cast<U>(ReadU32(addr));                       // estende zero
    } else {
        static_assert(sizeof(U) == 0, "tipo de argumento nao suportado");
    }
}

// Retorno host -> registradores do guest
template <typename R>
inline void SetReturn(GuestCPU& cpu, R v) {
    if constexpr (std::is_pointer_v<R>) {
        const uint64_t p = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(v));
        if ((p >> 32) != 0 && p != ~0ull)
            Win32_Log("ABI: ponteiro host %p nao cabe em 32 bits (fora da arena?)", reinterpret_cast<const void*>(v));
        cpu.eax = static_cast<uint32_t>(p);          // -1 -> 0xFFFFFFFF (INVALID_HANDLE_VALUE)
    } else if constexpr (std::is_floating_point_v<R>) {
        static_assert(sizeof(R) == 0, "retorno float/double (x87 st0) nao suportado");
    } else if constexpr (kIsWide<R>) {
        const uint64_t u = static_cast<uint64_t>(v);
        cpu.eax = static_cast<uint32_t>(u);
        cpu.edx = static_cast<uint32_t>(u >> 32);    // 64 bits => edx:eax
    } else if constexpr (std::is_integral_v<R> || std::is_enum_v<R>) {
        cpu.eax = static_cast<uint32_t>(v);
    } else {
        static_assert(sizeof(R) == 0, "tipo de retorno nao suportado");
    }
}

} // namespace win32abi

// ----------------------------------------------------------------------------
// GuestThunk<&funcao, CalleePops>::Invoke(cpu)
//   CalleePops = true  -> stdcall (callee limpa args: 'ret N')   [Win32 API]
//   CalleePops = false -> cdecl   (caller limpa; callee só faz 'ret')
// ----------------------------------------------------------------------------
template <auto Fn, bool CalleePops>
struct GuestThunk;

template <typename R, typename... Args, R (*Fn)(Args...), bool CalleePops>
struct GuestThunk<Fn, CalleePops> {
    static constexpr auto     kOffs     = win32abi::MakeOffsets<Args...>();
    static constexpr uint32_t kArgSlots = kOffs[sizeof...(Args)];
    static constexpr uint32_t kRetBytes = WIN32_GUEST_CALL_PUSHES_RETADDR ? 4u : 0u;

    static void Invoke(GuestCPU& cpu) {
        Run(cpu, cpu.esp + kRetBytes, std::index_sequence_for<Args...>{});
        cpu.esp += kRetBytes + (CalleePops ? 4u * kArgSlots : 0u);   // emula ret / ret N
    }

private:
    template <size_t... I>
    static void Run([[maybe_unused]] GuestCPU& cpu, [[maybe_unused]] uint32_t argBase,
                    std::index_sequence<I...>) {
        if constexpr (std::is_void_v<R>)
            Fn(win32abi::FromStack<Args>(argBase + 4u * kOffs[I])...);
        else
            win32abi::SetReturn(cpu, Fn(win32abi::FromStack<Args>(argBase + 4u * kOffs[I])...));
    }
};

// Registra o thunk no registry de exports (GetProcAddress / Win32_ResolveImport).
#define WIN32_STDCALL_EXPORT(dll, fn) \
    static Win32ExportRegistrar win32_registrar_##fn(dll, #fn, \
        reinterpret_cast<void*>(static_cast<GuestThunkFn>(&GuestThunk<&fn, true>::Invoke)))

#define WIN32_CDECL_EXPORT(dll, fn) \
    static Win32ExportRegistrar win32_registrar_##fn(dll, #fn, \
        reinterpret_cast<void*>(static_cast<GuestThunkFn>(&GuestThunk<&fn, false>::Invoke)))

// Chamada estática (o gerador de código já sabe qual import é):
//   WIN32_CALL_STDCALL(cpu, CreateFileA);
#define WIN32_CALL_STDCALL(cpu, fn) GuestThunk<&fn, true>::Invoke(cpu)
#define WIN32_CALL_CDECL(cpu, fn)   GuestThunk<&fn, false>::Invoke(cpu)

// Chamada dinâmica (call [reg]/call [IAT] com alvo só conhecido em runtime).
// Retorna false se 'guestTarget' não for um thunk (=> é código recompilado do jogo).
inline bool Win32_DispatchThunk(GuestCPU& cpu, uint32_t guestTarget) {
    auto fn = reinterpret_cast<GuestThunkFn>(Win32_ResolveThunk(guestTarget));
    if (!fn) return false;
    fn(cpu);
    return true;
}

// ----------------------------------------------------------------------------
// Configuração do sistema de arquivos (chame na inicialização, ex.: via JNI)
// ----------------------------------------------------------------------------
// Diretório Android que corresponde a C:\Games\PES2017  (padrão: /storage/emulated/0/PES2017)
extern "C" void Win32_SetAndroidRoot(const char* absDir);
// Mapeia um prefixo Windows qualquer para um diretório Android. Prefixos mais longos vencem.
//   Win32_AddPathMount("C:\\Users\\Player\\Documents\\KONAMI", "/data/data/<pkg>/files/save");
extern "C" void Win32_AddPathMount(const char* winPrefix, const char* androidDir);
// Apenas para depuração/testes. Retorna false se 'out' for pequeno demais.
extern "C" bool Win32_TranslatePath(const char* winPath, char* out, size_t outSize);
