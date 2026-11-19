#include <cstdint>

struct Pes17GeneratedState {
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t esp = 0;
    bool zf = false;
};

bool pes17_fn_047d5000(Pes17GeneratedState& state) {
    (void)state;
    goto block_047d5000;

block_047d5000:
    // 0x047d5000 push ebp
    // 0x047d5001 mov ebp, esp
    // 0x047d5003 lea esp, [esp - 4]
    // 0x047d500a push 0x40
    // 0x047d500c push 0x3000
    // 0x047d5011 push 0xfa00
    // 0x047d5016 push 0
    // 0x047d5018 call dword ptr [0x47d7304]
    // 0x047d501e lea ecx, [0x4696597]

block_047d5024:
    // 0x047d5024 cmp dword ptr [ecx], 0
    // 0x047d5027 je 0x47d5037

block_047d5029:
    // 0x047d5029 mov edx, dword ptr [ecx + 4]
    // 0x047d502c mov ebx, dword ptr [ecx]
    // 0x047d502e mov dword ptr [ebx], eax
    // 0x047d5030 add dword ptr [ebx], edx
    // 0x047d5032 add ecx, 8
    // 0x047d5035 jmp 0x47d5024

block_047d5037:
    // 0x047d5037 mov esp, ebp
    // 0x047d5039 pop ebp
    // 0x047d503a ret

    return true;
}
