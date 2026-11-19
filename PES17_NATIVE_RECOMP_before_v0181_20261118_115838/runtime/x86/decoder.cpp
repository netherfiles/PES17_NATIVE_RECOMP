#include "decoder.h"

#include <capstone/capstone.h>
#include <algorithm>

namespace pes17::x86 {

namespace {

bool hasGroup(
    csh handle,
    const cs_insn& insn,
    std::uint8_t group) {

    return cs_insn_group(handle, &insn, group);
}

}

Decoder::Decoder() {
    csh handle = 0;

    if (cs_open(
            CS_ARCH_X86,
            CS_MODE_32,
            &handle) != CS_ERR_OK) {
        return;
    }

    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);

    handle_ = static_cast<std::size_t>(handle);
    valid_ = true;
}

Decoder::~Decoder() {
    if (!valid_)
        return;

    csh handle = static_cast<csh>(handle_);
    cs_close(&handle);
}

bool Decoder::valid() const {
    return valid_;
}

std::vector<DecodedInstruction> Decoder::decode(
    const std::uint8_t* code,
    std::size_t size,
    std::uint64_t virtualAddress,
    std::size_t maxInstructions) const {

    std::vector<DecodedInstruction> result;

    if (!valid_ || !code || size == 0)
        return result;

    csh handle = static_cast<csh>(handle_);

    cs_insn* instructions = nullptr;

    const std::size_t count = cs_disasm(
        handle,
        code,
        size,
        virtualAddress,
        0,
        &instructions);

    if (!count)
        return result;

    const std::size_t limit =
        std::min(count, maxInstructions);

    result.reserve(limit);

    for (std::size_t i = 0; i < limit; ++i) {
        const cs_insn& src = instructions[i];

        DecodedInstruction out;

        out.address = src.address;
        out.size = static_cast<std::uint16_t>(src.size);
        out.instructionId =
            static_cast<std::uint32_t>(src.id);

        out.bytes.assign(
            src.bytes,
            src.bytes + src.size);

        out.mnemonic = src.mnemonic;
        out.operands = src.op_str;

        if (src.detail) {
            const auto& x86 = src.detail->x86;

            out.decodedOperands.reserve(
                x86.op_count);

            for (std::uint8_t opIndex = 0;
                 opIndex < x86.op_count;
                 ++opIndex) {

                const auto& sourceOp =
                    x86.operands[opIndex];

                DecodedInstruction::Operand operand;

                operand.size =
                    static_cast<std::uint8_t>(
                        sourceOp.size);

                switch (sourceOp.type) {
                    case X86_OP_REG:
                        operand.kind =
                            DecodedInstruction::Operand::Kind::Register;

                        operand.reg =
                            static_cast<std::uint32_t>(
                                sourceOp.reg);
                        break;

                    case X86_OP_IMM:
                        operand.kind =
                            DecodedInstruction::Operand::Kind::Immediate;

                        operand.immediate =
                            sourceOp.imm;
                        break;

                    case X86_OP_MEM:
                        operand.kind =
                            DecodedInstruction::Operand::Kind::Memory;

                        operand.memoryBase =
                            static_cast<std::uint32_t>(
                                sourceOp.mem.base);

                        operand.memoryIndex =
                            static_cast<std::uint32_t>(
                                sourceOp.mem.index);

                        operand.memoryScale =
                            sourceOp.mem.scale;

                        operand.memoryDisplacement =
                            sourceOp.mem.disp;
                        break;

                    default:
                        operand.kind =
                            DecodedInstruction::Operand::Kind::Invalid;
                        break;
                }

                out.decodedOperands.push_back(
                    operand);
            }
        }

        if (hasGroup(handle, src, CS_GRP_CALL)) {
            out.flow = FlowType::Call;
        } else if (hasGroup(handle, src, CS_GRP_RET)) {
            out.flow = FlowType::Return;
        } else if (src.id == X86_INS_JMP) {
            out.flow = FlowType::Jump;
        } else if (hasGroup(handle, src, CS_GRP_JUMP)) {
            out.flow = FlowType::ConditionalJump;
        }

        if (src.detail &&
            src.detail->x86.op_count > 0) {

            const auto& op =
                src.detail->x86.operands[0];

            if ((out.flow == FlowType::Call ||
                 out.flow == FlowType::Jump ||
                 out.flow == FlowType::ConditionalJump) &&
                op.type == X86_OP_IMM) {

                out.hasTarget = true;
                out.target =
                    static_cast<std::uint64_t>(op.imm);
            }
        }

        result.push_back(std::move(out));
    }

    cs_free(instructions, count);
    return result;
}

const char* flowName(FlowType type) {
    switch (type) {
        case FlowType::Normal:
            return "NORMAL";
        case FlowType::Call:
            return "CALL";
        case FlowType::Jump:
            return "JMP";
        case FlowType::ConditionalJump:
            return "JCC";
        case FlowType::Return:
            return "RET";
    }

    return "?";
}

}
