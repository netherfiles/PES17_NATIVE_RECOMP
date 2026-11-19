#include <cstdint>
#include "runtime/execution/machine_state.h"

bool pes17_fn_047d5000(pes17::execution::MachineState& state) {
    goto block_047d5024;

block_047d5024: {
        // 0x047d5024 cmp dword ptr [ecx], 0
        const auto lhs_047d5024_0_read = state.memory.read32((state.reg(pes17::ir::Register::ECX)));
        if (!lhs_047d5024_0_read) return false;
        const std::uint32_t lhs_047d5024_0 = *lhs_047d5024_0_read;
        const std::uint32_t rhs_047d5024_0 = 0x0u;
        const std::uint32_t value_047d5024_0 = lhs_047d5024_0 - rhs_047d5024_0;
        state.flags.cf = lhs_047d5024_0 < rhs_047d5024_0;
        state.flags.of = (((lhs_047d5024_0 ^ rhs_047d5024_0) & (lhs_047d5024_0 ^ value_047d5024_0)) & 0x80000000u) != 0;
        state.flags.zf = value_047d5024_0 == 0;
        state.flags.sf = (value_047d5024_0 & 0x80000000u) != 0;
        // 0x047d5027 je 0x47d5037
        if (state.flags.zf) goto block_047d5037;
        goto block_047d5029;
    }

block_047d5029: {
        // 0x047d5029 mov edx, dword ptr [ecx + 4]
        const auto source_047d5029_0_read = state.memory.read32((state.reg(pes17::ir::Register::ECX) + 0x4u));
        if (!source_047d5029_0_read) return false;
        const std::uint32_t source_047d5029_0 = *source_047d5029_0_read;
        state.reg(pes17::ir::Register::EDX) = source_047d5029_0;
        // 0x047d502c mov ebx, dword ptr [ecx]
        const auto source_047d502c_1_read = state.memory.read32((state.reg(pes17::ir::Register::ECX)));
        if (!source_047d502c_1_read) return false;
        const std::uint32_t source_047d502c_1 = *source_047d502c_1_read;
        state.reg(pes17::ir::Register::EBX) = source_047d502c_1;
        // 0x047d502e mov dword ptr [ebx], eax
        const std::uint32_t source_047d502e_2 = state.reg(pes17::ir::Register::EAX);
        if (!state.memory.write32((state.reg(pes17::ir::Register::EBX)), source_047d502e_2)) return false;
        // 0x047d5030 add dword ptr [ebx], edx
        const auto lhs_047d5030_3_read = state.memory.read32((state.reg(pes17::ir::Register::EBX)));
        if (!lhs_047d5030_3_read) return false;
        const std::uint32_t lhs_047d5030_3 = *lhs_047d5030_3_read;
        const std::uint32_t rhs_047d5030_3 = state.reg(pes17::ir::Register::EDX);
        const std::uint32_t value_047d5030_3 = lhs_047d5030_3 + rhs_047d5030_3;
        const std::uint64_t wide_047d5030_3 = static_cast<std::uint64_t>(lhs_047d5030_3) + rhs_047d5030_3;
        state.flags.cf = wide_047d5030_3 > 0xffffffffULL;
        state.flags.of = ((~(lhs_047d5030_3 ^ rhs_047d5030_3) & (lhs_047d5030_3 ^ value_047d5030_3)) & 0x80000000u) != 0;
        state.flags.zf = value_047d5030_3 == 0;
        state.flags.sf = (value_047d5030_3 & 0x80000000u) != 0;
        if (!state.memory.write32((state.reg(pes17::ir::Register::EBX)), value_047d5030_3)) return false;
        // 0x047d5032 add ecx, 8
        const std::uint32_t lhs_047d5032_4 = state.reg(pes17::ir::Register::ECX);
        const std::uint32_t rhs_047d5032_4 = 0x8u;
        const std::uint32_t value_047d5032_4 = lhs_047d5032_4 + rhs_047d5032_4;
        const std::uint64_t wide_047d5032_4 = static_cast<std::uint64_t>(lhs_047d5032_4) + rhs_047d5032_4;
        state.flags.cf = wide_047d5032_4 > 0xffffffffULL;
        state.flags.of = ((~(lhs_047d5032_4 ^ rhs_047d5032_4) & (lhs_047d5032_4 ^ value_047d5032_4)) & 0x80000000u) != 0;
        state.flags.zf = value_047d5032_4 == 0;
        state.flags.sf = (value_047d5032_4 & 0x80000000u) != 0;
        state.reg(pes17::ir::Register::ECX) = value_047d5032_4;
        // 0x047d5035 jmp 0x47d5024
        goto block_047d5024;
    }

block_047d5037: {
        // 0x047d5037 mov esp, ebp
        const std::uint32_t source_047d5037_0 = state.reg(pes17::ir::Register::EBP);
        state.reg(pes17::ir::Register::ESP) = source_047d5037_0;
        // 0x047d5039 pop ebp
        const auto pop_047d5039_1 = state.memory.read32(state.reg(pes17::ir::Register::ESP));
        if (!pop_047d5039_1) return false;
        state.reg(pes17::ir::Register::EBP) = *pop_047d5039_1;
        state.reg(pes17::ir::Register::ESP) += 4u;
        // 0x047d503a ret
        return true;
    }

    return false;
}
