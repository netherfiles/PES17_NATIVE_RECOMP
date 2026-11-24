#pragma once
// ============================================================================
// win32_abi.h - Ponte ABI: pilha do guest x86 (stdcall/cdecl) -> funções C++
// ============================================================================

#include "win32_types.h"

#include <array>
#include <cstring> // ADICIONADO PARA O MEMCPY
#include <type_traits>
#include <utility>

#ifndef WIN32_GUEST_CALL_PUSHES_RETADDR
#define WIN32_GUEST_CALL_PUSHES_RETADDR 1
#endif

#ifndef WIN32_ABI_CUSTOM_CPU
struct GuestCPU {
    uint32_t eax = 0, ecx = 0, edx = 0, ebx = 0;
    uint32_t esp = 0, ebp = 0, esi = 0, edi = 0;
};
#endif

typedef void (*GuestThunkFn)(GuestCPU&);

namespace win32abi {

inline uint32_t ReadU32(uint32_t guestAddr) {
    uint32_t v;
    std::memcpy(&v, reinterpret_cast<const void*>(static_cast<uintptr_t>(guestAddr)), sizeof(v));
    return v;
}

template <typename T>
inline constexpr bool kIsWide = std::is_same_v<T, long long> ||
                                std::is_same_v<T, unsigned long long> ||
                                std::is_same_v<T, double>;

template <typename T>
inline constexpr uint32_t kSlots = kIsWide<T> ? 2u : 1u;

template <typename... A>
constexpr std::array<uint32_t, sizeof...(A) + 1> MakeOffsets() {
    std::array<uint32_t, sizeof...(A) + 1> a{};
    const uint32_t s[sizeof...(A) + 1] = { kSlots<A>..., 0u };
    uint32_t o = 0;
    for (size_t i = 0; i < sizeof...(A); ++i) { a[i] = o; o += s[i]; }
    a[sizeof...(A)] = o;
    return a;
}

template <typename T>
inline T FromStack(uint32_t addr) {
    using U = std::remove_cv_t<T>;
    static_assert(!std::is_class_v<U> && !std::is_union_v<U>, "struct por valor nao suportada");
    
    // CORRIGIDO: Trocado std::bit_cast por std::memcpy
    if constexpr (std::is_same_v<U, double>) {
        const uint64_t v = ReadU32(addr) | (static_cast<uint64_t>(ReadU32(addr + 4)) << 32);
        double res;
        std::memcpy(&res, &v, sizeof(double));
        return res;
    } else if constexpr (std::is_same_v<U, float>) {
        const uint32_t u32 = ReadU32(addr);
        float res;
        std::memcpy(&res, &u32, sizeof(float));
        return res;
    } else if constexpr (std::is_pointer_v<U>) {
        const uint32_t v = ReadU32(addr);
        if (v == 0xFFFFFFFFu) return reinterpret_cast<U>(static_cast<intptr_t>(-1));
        return reinterpret_cast<U>(static_cast<uintptr_t>(v));
    } else if constexpr (kIsWide<U>) {
        const uint64_t v = ReadU32(addr) | (static_cast<uint64_t>(ReadU32(addr + 4)) << 32);
        return static_cast<U>(v);
    } else if constexpr (std::is_integral_v<U> || std::is_enum_v<U>) {
        if constexpr (std::is_signed_v<U>) return static_cast<U>(static_cast<int32_t>(ReadU32(addr)));
        else                               return static_cast<U>(ReadU32(addr));
    } else {
        static_assert(sizeof(U) == 0, "tipo de argumento nao suportado");
    }
}

template <typename R>
inline void SetReturn(GuestCPU& cpu, R v) {
    if constexpr (std::is_pointer_v<R>) {
        const uint64_t p = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(v));
        if ((p >> 32) != 0 && p != ~0ull)
            Win32_Log("ABI: ponteiro host %p nao cabe em 32 bits (fora da arena?)", reinterpret_cast<const void*>(v));
        cpu.eax = static_cast<uint32_t>(p);
    } else if constexpr (std::is_floating_point_v<R>) {
        static_assert(sizeof(R) == 0, "retorno float/double (x87 st0) nao suportado");
    } else if constexpr (kIsWide<R>) {
        const uint64_t u = static_cast<uint64_t>(v);
        cpu.eax = static_cast<uint32_t>(u);
        cpu.edx = static_cast<uint32_t>(u >> 32);
    } else if constexpr (std::is_integral_v<R> || std::is_enum_v<R>) {
        cpu.eax = static_cast<uint32_t>(v);
    } else {
        static_assert(sizeof(R) == 0, "tipo de retorno nao suportado");
    }
}

} // namespace win32abi

template <auto Fn, bool CalleePops>
struct GuestThunk;

template <typename R, typename... Args, R (*Fn)(Args...), bool CalleePops>
struct GuestThunk<Fn, CalleePops> {
    static constexpr auto     kOffs     = win32abi::MakeOffsets<Args...>();
    static constexpr uint32_t kArgSlots = kOffs[sizeof...(Args)];
    static constexpr uint32_t kRetBytes = WIN32_GUEST_CALL_PUSHES_RETADDR ? 4u : 0u;

    static void Invoke(GuestCPU& cpu) {
        Run(cpu, cpu.esp + kRetBytes, std::index_sequence_for<Args...>{});
        cpu.esp += kRetBytes + (CalleePops ? 4u * kArgSlots : 0u);
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

#define WIN32_STDCALL_EXPORT(dll, fn) \
    static Win32ExportRegistrar win32_registrar_##fn(dll, #fn, \
        reinterpret_cast<void*>(static_cast<GuestThunkFn>(&GuestThunk<&fn, true>::Invoke)))

#define WIN32_CDECL_EXPORT(dll, fn) \
    static Win32ExportRegistrar win32_registrar_##fn(dll, #fn, \
        reinterpret_cast<void*>(static_cast<GuestThunkFn>(&GuestThunk<&fn, false>::Invoke)))

#define WIN32_CALL_STDCALL(cpu, fn) GuestThunk<&fn, true>::Invoke(cpu)
#define WIN32_CALL_CDECL(cpu, fn)   GuestThunk<&fn, false>::Invoke(cpu)

inline bool Win32_DispatchThunk(GuestCPU& cpu, uint32_t guestTarget) {
    auto fn = reinterpret_cast<GuestThunkFn>(Win32_ResolveThunk(guestTarget));
    if (!fn) return false;
    fn(cpu);
    return true;
}

extern "C" void Win32_SetAndroidRoot(const char* absDir);
extern "C" void Win32_AddPathMount(const char* winPrefix, const char* androidDir);
extern "C" bool Win32_TranslatePath(const char* winPath, char* out, size_t outSize);
