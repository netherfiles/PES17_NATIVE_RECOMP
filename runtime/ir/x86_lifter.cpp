#include "x86_lifter.h"
#include <capstone/x86.h>

namespace pes17::ir {

namespace {
Register mapRegister(std::uint32_t reg) {
    switch (reg) {
        case X86_REG_EAX: return Register::EAX;
        case X86_REG_EBX: return Register::EBX;
        case X86_REG_ECX: return Register::ECX;
        case X86_REG_EDX: return Register::EDX;
        case X86_REG_ESI: return Register::ESI;
        case X86_REG_EDI: return Register::EDI;
        case X86_REG_EBP: return Register::EBP;
        case X86_REG_ESP: return Register::ESP;
        default: return Register::None;
    }
}

Operand liftOperand(const x86::DecodedInstruction::Operand& source) {
    Operand result;
    result.width = static_cast<std::uint8_t>(source.size * 8);
    using SourceKind = x86::DecodedInstruction::Operand::Kind;

    switch (source.kind) {
        case SourceKind::Register:
            result.kind = OperandKind::Register;
            result.reg = mapRegister(source.reg);
            break;
        case SourceKind::Immediate:
            result.kind = OperandKind::Immediate;
            result.immediate = source.immediate;
            break;
        case SourceKind::Memory:
            result.kind = OperandKind::Memory;
            result.memory.base = mapRegister(source.memoryBase);
            result.memory.index = mapRegister(source.memoryIndex);
            result.memory.scale = static_cast<std::uint8_t>(source.memoryScale);
            result.memory.displacement = source.memoryDisplacement;
            result.memory.width = result.width;
            break;
        case SourceKind::Invalid:
            result.kind = OperandKind::None;
            break;
    }
    return result;
}
}

Instruction X86Lifter::liftInstruction(const x86::DecodedInstruction& source) const {
    Instruction result;
    result.sourceAddress = source.address;
    result.originalMnemonic = source.mnemonic;
    result.originalOperands = source.operands;
    result.hasTarget = source.hasTarget;
    result.target = source.target;
    result.operands.reserve(source.decodedOperands.size());

    for (const auto& operand : source.decodedOperands) {
        result.operands.push_back(liftOperand(operand));
    }

    auto setMathFlags = [&]() {
        result.flagsWritten = static_cast<std::uint8_t>(Flag::ZF) | static_cast<std::uint8_t>(Flag::CF) | static_cast<std::uint8_t>(Flag::SF) | static_cast<std::uint8_t>(Flag::OF);
    };

    switch (source.instructionId) {
        case X86_INS_PUSH: result.opcode = Opcode::Push; break;
        case X86_INS_POP: result.opcode = Opcode::Pop; break;
        case X86_INS_MOV:
            if (result.operands.size() >= 2) {
                const auto dstKind = result.operands[0].kind;
                const auto srcKind = result.operands[1].kind;
                if (dstKind == OperandKind::Register && srcKind == OperandKind::Memory) result.opcode = Opcode::Load;
                else if (dstKind == OperandKind::Memory && (srcKind == OperandKind::Register || srcKind == OperandKind::Immediate)) result.opcode = Opcode::Store;
                else result.opcode = Opcode::Move;
            } else result.opcode = Opcode::Move;
            break;
        case X86_INS_LEA: result.opcode = Opcode::Address; break;
        case X86_INS_ADD: result.opcode = Opcode::Add; setMathFlags(); break;
        case X86_INS_SUB: result.opcode = Opcode::Subtract; setMathFlags(); break;
        case X86_INS_CMP: result.opcode = Opcode::Compare; setMathFlags(); break;
        case X86_INS_CALL: result.opcode = source.hasTarget ? Opcode::CallDirect : Opcode::CallIndirect; break;
        case X86_INS_JMP: result.opcode = Opcode::Branch; break;
        case X86_INS_JE: result.opcode = Opcode::BranchIfEqual; result.flagsRead = static_cast<std::uint8_t>(Flag::ZF); break;
        case X86_INS_JNE: result.opcode = Opcode::BranchIfNotEqual; result.flagsRead = static_cast<std::uint8_t>(Flag::ZF); break;
        case X86_INS_RET:
        case X86_INS_RETF: result.opcode = Opcode::Return; break;
        
        case X86_INS_INC: result.opcode = Opcode::Increment; setMathFlags(); break;
        case X86_INS_XOR: result.opcode = Opcode::Xor; setMathFlags(); break;
        case X86_INS_SHL: result.opcode = Opcode::ShiftLeft; setMathFlags(); break;
        case X86_INS_TEST: result.opcode = Opcode::Test; setMathFlags(); break;
        case X86_INS_MOVZX: result.opcode = Opcode::MoveZeroExtend; break;
        case X86_INS_JA: result.opcode = Opcode::BranchIfAbove; break;
        case X86_INS_SETE: result.opcode = Opcode::SetIfEqual; result.flagsRead = static_cast<std::uint8_t>(Flag::ZF); break;
        case X86_INS_SBB: result.opcode = Opcode::SubtractWithBorrow; setMathFlags(); break;
        case X86_INS_XCHG: result.opcode = Opcode::Exchange; break;
        case X86_INS_PUSHFD: result.opcode = Opcode::PushFlags; break;
        case X86_INS_FCMOVU: result.opcode = Opcode::Fcmovu; break;

        // Instruções do Claude (Matemática e Ponto Flutuante)
        case X86_INS_AND: result.opcode = Opcode::And; setMathFlags(); break;
        case X86_INS_OR: result.opcode = Opcode::Or; setMathFlags(); break;
        case X86_INS_SHR: result.opcode = Opcode::ShiftRight; setMathFlags(); break;
        case X86_INS_SAR: result.opcode = Opcode::ShiftRight; setMathFlags(); break;
        
        case X86_INS_FLD: result.opcode = Opcode::PushFpu; break;
        case X86_INS_FILD: result.opcode = Opcode::PushFpu; break;
        case X86_INS_FST: result.opcode = Opcode::PopFpu; break;
        case X86_INS_FSTP: result.opcode = Opcode::PopFpu; break;
        case X86_INS_FADD: result.opcode = Opcode::AddFpu; break;
        case X86_INS_FMUL: result.opcode = Opcode::MulFpu; break;
        case X86_INS_FDIV: result.opcode = Opcode::DivFpu; break;

        case X86_INS_MOVAPS: result.opcode = Opcode::MoveAligned; break;
        case X86_INS_MOVSS: result.opcode = Opcode::MoveAligned; break;

        default: result.opcode = Opcode::Unsupported; break;
    }
    return result;
}

Function X86Lifter::lift(const analysis::Function& source) const {
    Function result;
    result.entry = source.entry;
    for (const auto& [address, sourceBlock] : source.blocks) {
        Block block;
        block.start = address;
        block.successors = sourceBlock.successors;
        for (const auto& sourceInstruction : sourceBlock.instructions) {
            auto instruction = liftInstruction(sourceInstruction);
            if (instruction.opcode == Opcode::Unsupported) ++result.unsupportedCount;
            block.instructions.push_back(std::move(instruction));
        }
        result.blocks.push_back(std::move(block));
    }
    return result;
}

} // namespace pes17::ir
