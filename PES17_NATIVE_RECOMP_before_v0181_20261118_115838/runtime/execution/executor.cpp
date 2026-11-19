#include "executor.h"

namespace pes17::execution {

namespace {

void updateAddFlags(
    MachineState& state,
    std::uint32_t lhs,
    std::uint32_t rhs,
    std::uint32_t value) {

    const std::uint64_t wide =
        static_cast<std::uint64_t>(lhs) + rhs;

    state.flags.zf = value == 0;
    state.flags.sf = (value & 0x80000000u) != 0;
    state.flags.cf = wide > 0xffffffffULL;

    state.flags.of =
        ((~(lhs ^ rhs) & (lhs ^ value)) &
         0x80000000u) != 0;
}

void updateSubFlags(
    MachineState& state,
    std::uint32_t lhs,
    std::uint32_t rhs,
    std::uint32_t value) {

    state.flags.zf = value == 0;
    state.flags.sf = (value & 0x80000000u) != 0;
    state.flags.cf = lhs < rhs;

    state.flags.of =
        (((lhs ^ rhs) & (lhs ^ value)) &
         0x80000000u) != 0;
}

}

Executor::Executor(MachineState& state)
    : state_(state) {
}

std::optional<std::uint32_t>
Executor::effectiveAddress(
    const ir::MemoryOperand& memory) const {

    std::uint64_t address = 0;

    if (memory.base != ir::Register::None) {
        address += state_.reg(memory.base);
    }

    if (memory.index != ir::Register::None) {
        address +=
            static_cast<std::uint64_t>(
                state_.reg(memory.index)) *
            memory.scale;
    }

    address +=
        static_cast<std::uint64_t>(
            memory.displacement);

    if (address > 0xffffffffULL)
        return std::nullopt;

    return static_cast<std::uint32_t>(address);
}

std::optional<std::uint32_t>
Executor::readOperand(
    const ir::Operand& operand) {

    switch (operand.kind) {
        case ir::OperandKind::Register:
            return state_.reg(operand.reg);

        case ir::OperandKind::Immediate:
            return static_cast<std::uint32_t>(
                operand.immediate);

        case ir::OperandKind::Memory: {
            if (operand.width != 32)
                return std::nullopt;

            const auto address =
                effectiveAddress(operand.memory);

            if (!address)
                return std::nullopt;

            return state_.memory.read32(*address);
        }

        case ir::OperandKind::None:
            break;
    }

    return std::nullopt;
}

bool Executor::writeOperand(
    const ir::Operand& operand,
    std::uint32_t value) {

    switch (operand.kind) {
        case ir::OperandKind::Register:
            state_.reg(operand.reg) = value;
            return true;

        case ir::OperandKind::Memory: {
            if (operand.width != 32)
                return false;

            const auto address =
                effectiveAddress(operand.memory);

            if (!address)
                return false;

            return state_.memory.write32(
                *address,
                value);
        }

        case ir::OperandKind::Immediate:
        case ir::OperandKind::None:
            break;
    }

    return false;
}

ExecutionResult Executor::executeBlock(
    const ir::Block& block) {

    ExecutionResult result;

    for (const auto& instruction :
         block.instructions) {

        ++result.instructionsExecuted;

        switch (instruction.opcode) {
            case ir::Opcode::Push: {
                if (instruction.operands.empty()) {
                    result.success = false;
                    result.error = "PUSH missing operand";
                    return result;
                }

                const auto value =
                    readOperand(instruction.operands[0]);

                if (!value) {
                    result.success = false;
                    result.error = "PUSH operand read failure";
                    return result;
                }

                auto& esp =
                    state_.reg(ir::Register::ESP);

                esp -= 4;

                if (!state_.memory.write32(esp, *value)) {
                    result.success = false;
                    result.error = "PUSH stack write failure";
                    return result;
                }

                break;
            }

            case ir::Opcode::Pop: {
                if (instruction.operands.empty()) {
                    result.success = false;
                    result.error = "POP missing operand";
                    return result;
                }

                auto& esp =
                    state_.reg(ir::Register::ESP);

                const auto value =
                    state_.memory.read32(esp);

                if (!value) {
                    result.success = false;
                    result.error = "POP stack read failure";
                    return result;
                }

                if (!writeOperand(
                        instruction.operands[0],
                        *value)) {
                    result.success = false;
                    result.error = "POP destination failure";
                    return result;
                }

                esp += 4;
                break;
            }

            case ir::Opcode::Move:
            case ir::Opcode::Load:
            case ir::Opcode::Store: {
                if (instruction.operands.size() < 2) {
                    result.success = false;
                    result.error =
                        "Move/Load/Store missing operands";
                    return result;
                }

                const auto value =
                    readOperand(
                        instruction.operands[1]);

                if (!value ||
                    !writeOperand(
                        instruction.operands[0],
                        *value)) {

                    result.success = false;
                    result.error =
                        "Move/Load/Store operand failure";
                    return result;
                }

                break;
            }

            case ir::Opcode::Add:
            case ir::Opcode::Subtract:
            case ir::Opcode::Compare: {
                if (instruction.operands.size() < 2) {
                    result.success = false;
                    result.error =
                        "ALU instruction missing operands";
                    return result;
                }

                const auto lhs =
                    readOperand(instruction.operands[0]);

                const auto rhs =
                    readOperand(instruction.operands[1]);

                if (!lhs || !rhs) {
                    result.success = false;
                    result.error =
                        "ALU operand read failure";
                    return result;
                }

                if (instruction.opcode ==
                    ir::Opcode::Add) {

                    const std::uint32_t value =
                        *lhs + *rhs;

                    updateAddFlags(
                        state_,
                        *lhs,
                        *rhs,
                        value);

                    if (!writeOperand(
                            instruction.operands[0],
                            value)) {
                        result.success = false;
                        result.error =
                            "ADD destination write failure";
                        return result;
                    }

                } else {
                    const std::uint32_t value =
                        *lhs - *rhs;

                    updateSubFlags(
                        state_,
                        *lhs,
                        *rhs,
                        value);

                    if (instruction.opcode ==
                        ir::Opcode::Subtract) {

                        if (!writeOperand(
                                instruction.operands[0],
                                value)) {
                            result.success = false;
                            result.error =
                                "SUB destination write failure";
                            return result;
                        }
                    }
                }

                break;
            }

            case ir::Opcode::Branch:
                if (!instruction.hasTarget) {
                    result.success = false;
                    result.error =
                        "Direct branch has no target";
                    return result;
                }

                result.nextBlock =
                    instruction.target;

                return result;

            case ir::Opcode::BranchIfEqual:
            case ir::Opcode::BranchIfNotEqual: {
                if (!instruction.hasTarget) {
                    result.success = false;
                    result.error =
                        "Conditional branch has no target";
                    return result;
                }

                const bool take =
                    instruction.opcode ==
                        ir::Opcode::BranchIfEqual
                    ? state_.flags.zf
                    : !state_.flags.zf;

                if (take) {
                    result.nextBlock =
                        instruction.target;
                } else if (
                    block.successors.size() == 2) {

                    for (const auto successor :
                         block.successors) {

                        if (successor !=
                            instruction.target) {
                            result.nextBlock =
                                successor;
                            break;
                        }
                    }
                }

                return result;
            }

            case ir::Opcode::Return:
                result.returned = true;
                return result;

            default:
                result.success = false;
                result.error =
                    "Opcode not implemented by executor";
                return result;
        }
    }

    return result;
}

}
