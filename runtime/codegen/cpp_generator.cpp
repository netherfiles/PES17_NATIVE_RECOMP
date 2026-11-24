#include "cpp_generator.h"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>

namespace pes17::codegen {

namespace {

std::string hexAddress(std::uint64_t address) {
    std::ostringstream out;
    out << std::hex << std::setw(8) << std::setfill('0') << address;
    return out.str();
}

std::string blockLabel(std::uint64_t address) {
    return "block_" + hexAddress(address);
}

std::string regExpr(ir::Register reg) {
    using R = ir::Register;
    switch (reg) {
        case R::EAX: return "state.reg(pes17::ir::Register::EAX)";
        case R::EBX: return "state.reg(pes17::ir::Register::EBX)";
        case R::ECX: return "state.reg(pes17::ir::Register::ECX)";
        case R::EDX: return "state.reg(pes17::ir::Register::EDX)";
        case R::ESI: return "state.reg(pes17::ir::Register::ESI)";
        case R::EDI: return "state.reg(pes17::ir::Register::EDI)";
        case R::EBP: return "state.reg(pes17::ir::Register::EBP)";
        case R::ESP: return "state.reg(pes17::ir::Register::ESP)";
        case R::None: break;
    }
    return {};
}

std::string immediateExpr(std::int64_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << static_cast<std::uint32_t>(value) << "u";
    return out.str();
}

std::string addressExpr(const ir::MemoryOperand& memory) {
    std::string expression;
    if (memory.base != ir::Register::None)
        expression = regExpr(memory.base);

    if (memory.index != ir::Register::None) {
        if (!expression.empty()) expression += " + ";
        expression += "(" + regExpr(memory.index) + " * " +
            std::to_string(memory.scale) + "u)";
    }

    if (memory.displacement != 0) {
        if (!expression.empty()) expression += " + ";
        expression += immediateExpr(memory.displacement);
    }

    if (expression.empty()) expression = "0u";
    return "(" + expression + ")";
}

bool validOperand(const ir::Operand& operand, std::string& error) {
    if (operand.width != 0 && operand.width != 32) {
        error = "Only 32-bit operands are supported";
        return false;
    }
    if (operand.kind == ir::OperandKind::Register && regExpr(operand.reg).empty()) {
        error = "Unsupported register operand";
        return false;
    }
    if (operand.kind == ir::OperandKind::Memory &&
        operand.memory.base == ir::Register::None &&
        operand.memory.index == ir::Register::None &&
        operand.memory.displacement == 0) {
        error = "Invalid empty memory operand";
        return false;
    }
    return true;
}

bool emitRead(
    std::ostringstream& out,
    const ir::Operand& operand,
    const std::string& variable,
    std::string& error) {

    if (!validOperand(operand, error)) return false;

    switch (operand.kind) {
        case ir::OperandKind::Register:
            out << "        const std::uint32_t " << variable << " = "
                << regExpr(operand.reg) << ";\n";
            return true;
        case ir::OperandKind::Immediate:
            out << "        const std::uint32_t " << variable << " = "
                << immediateExpr(operand.immediate) << ";\n";
            return true;
        case ir::OperandKind::Memory:
            out << "        const auto " << variable << "_read = state.memory.read32("
                << addressExpr(operand.memory) << ");\n"
                << "        if (!" << variable << "_read) return false;\n"
                << "        const std::uint32_t " << variable << " = *"
                << variable << "_read;\n";
            return true;
        case ir::OperandKind::None:
            error = "Missing operand";
            return false;
    }
    error = "Unknown operand kind";
    return false;
}

bool emitWrite(
    std::ostringstream& out,
    const ir::Operand& operand,
    const std::string& value,
    std::string& error) {

    if (!validOperand(operand, error)) return false;

    switch (operand.kind) {
        case ir::OperandKind::Register:
            out << "        " << regExpr(operand.reg) << " = " << value << ";\n";
            return true;
        case ir::OperandKind::Memory:
            out << "        if (!state.memory.write32(" << addressExpr(operand.memory)
                << ", " << value << ")) return false;\n";
            return true;
        case ir::OperandKind::Immediate:
            error = "Cannot write to an immediate operand";
            return false;
        case ir::OperandKind::None:
            error = "Missing destination operand";
            return false;
    }
    error = "Unknown destination operand";
    return false;
}

const ir::Block* findBlock(const ir::Function& function, std::uint64_t address) {
    for (const auto& block : function.blocks)
        if (block.start == address) return &block;
    return nullptr;
}

std::set<std::uint64_t> reachableBlocks(
    const ir::Function& function,
    std::uint64_t start,
    std::string& error) {

    std::set<std::uint64_t> reachable;
    std::deque<std::uint64_t> pending{start};

    while (!pending.empty()) {
        const auto address = pending.front();
        pending.pop_front();
        if (!reachable.insert(address).second) continue;

        const auto* block = findBlock(function, address);
        if (!block) {
            error = "Reachable block not found: 0x" + hexAddress(address);
            return {};
        }
        for (const auto successor : block->successors)
            if (!reachable.contains(successor)) pending.push_back(successor);
    }
    return reachable;
}

bool requireOperands(
    const ir::Instruction& instruction,
    std::size_t count,
    std::string& error) {
    if (instruction.operands.size() < count) {
        error = "Instruction at 0x" + hexAddress(instruction.sourceAddress) +
            " has too few operands";
        return false;
    }
    return true;
}

bool emitInstruction(
    std::ostringstream& out,
    const ir::Block& block,
    const ir::Instruction& instruction,
    std::size_t index,
    std::string& error) {

    const auto suffix = hexAddress(instruction.sourceAddress) + "_" + std::to_string(index);
    out << "        // 0x" << hexAddress(instruction.sourceAddress) << " "
        << instruction.originalMnemonic;
    if (!instruction.originalOperands.empty())
        out << " " << instruction.originalOperands;
    out << "\n";

    switch (instruction.opcode) {
        case ir::Opcode::Move:
        case ir::Opcode::Load:
        case ir::Opcode::Store: {
            if (!requireOperands(instruction, 2, error)) return false;
            if (!emitRead(out, instruction.operands[1], "source_" + suffix, error)) return false;
            return emitWrite(out, instruction.operands[0], "source_" + suffix, error);
        }

        case ir::Opcode::Address: {
            if (!requireOperands(instruction, 2, error)) return false;
            if (instruction.operands[1].kind != ir::OperandKind::Memory) {
                error = "Address source is not memory at 0x" + hexAddress(instruction.sourceAddress);
                return false;
            }
            return emitWrite(
                out,
                instruction.operands[0],
                "static_cast<std::uint32_t>(" + addressExpr(instruction.operands[1].memory) + ")",
                error);
        }

        case ir::Opcode::Add:
        case ir::Opcode::Subtract:
        case ir::Opcode::Compare: {
            if (!requireOperands(instruction, 2, error)) return false;
            if (!emitRead(out, instruction.operands[0], "lhs_" + suffix, error)) return false;
            if (!emitRead(out, instruction.operands[1], "rhs_" + suffix, error)) return false;
            const bool add = instruction.opcode == ir::Opcode::Add;
            out << "        const std::uint32_t value_" << suffix << " = lhs_" << suffix
                << (add ? " + " : " - ") << "rhs_" << suffix << ";\n";
            if (add) {
                out << "        const std::uint64_t wide_" << suffix
                    << " = static_cast<std::uint64_t>(lhs_" << suffix << ") + rhs_"
                    << suffix << ";\n"
                    << "        state.flags.cf = wide_" << suffix << " > 0xffffffffULL;\n"
                    << "        state.flags.of = ((~(lhs_" << suffix << " ^ rhs_" << suffix
                    << ") & (lhs_" << suffix << " ^ value_" << suffix
                    << ")) & 0x80000000u) != 0;\n";
            } else {
                out << "        state.flags.cf = lhs_" << suffix << " < rhs_" << suffix << ";\n"
                    << "        state.flags.of = (((lhs_" << suffix << " ^ rhs_" << suffix
                    << ") & (lhs_" << suffix << " ^ value_" << suffix
                    << ")) & 0x80000000u) != 0;\n";
            }
            out << "        state.flags.zf = value_" << suffix << " == 0;\n"
                << "        state.flags.sf = (value_" << suffix << " & 0x80000000u) != 0;\n";
            if (instruction.opcode != ir::Opcode::Compare)
                return emitWrite(out, instruction.operands[0], "value_" + suffix, error);
            return true;
        }

        // ====================================================================
        // AQUI ESTÃO AS INSTRUÇÕES NOVAS QUE ESTAVAM BLOQUEANDO O JOGO
        // ====================================================================
        case ir::Opcode::Xor: {
            if (!requireOperands(instruction, 2, error)) return false;
            if (!emitRead(out, instruction.operands[0], "lhs_" + suffix, error)) return false;
            if (!emitRead(out, instruction.operands[1], "rhs_" + suffix, error)) return false;
            out << "        const std::uint32_t value_" << suffix << " = lhs_" << suffix << " ^ rhs_" << suffix << ";\n";
            out << "        state.flags.zf = value_" << suffix << " == 0;\n"
                << "        state.flags.cf = false;\n"
                << "        state.flags.of = false;\n"
                << "        state.flags.sf = (value_" << suffix << " & 0x80000000u) != 0;\n";
            return emitWrite(out, instruction.operands[0], "value_" + suffix, error);
        }

        case ir::Opcode::Exchange: {
            if (!requireOperands(instruction, 2, error)) return false;
            if (!emitRead(out, instruction.operands[0], "val1_" + suffix, error)) return false;
            if (!emitRead(out, instruction.operands[1], "val2_" + suffix, error)) return false;
            if (!emitWrite(out, instruction.operands[0], "val2_" + suffix, error)) return false;
            return emitWrite(out, instruction.operands[1], "val1_" + suffix, error);
        }

        case ir::Opcode::PushFlags: {
            out << "        state.reg(pes17::ir::Register::ESP) -= 4u;\n"
                << "        const std::uint32_t eflags_" << suffix << " = (state.flags.cf ? 1u : 0u) | (state.flags.zf ? 64u : 0u) | (state.flags.sf ? 128u : 0u) | (state.flags.of ? 2048u : 0u) | 2u;\n"
                << "        if (!state.memory.write32(state.reg(pes17::ir::Register::ESP), eflags_" << suffix << ")) return false;\n";
            return true;
        }

        case ir::Opcode::Branch:
            if (!instruction.hasTarget) {
                error = "Direct branch without target at 0x" + hexAddress(instruction.sourceAddress);
                return false;
            }
            out << "        goto " << blockLabel(instruction.target) << ";\n";
            return true;

        case ir::Opcode::BranchIfEqual:
        case ir::Opcode::BranchIfNotEqual: {
            if (!instruction.hasTarget) {
                error = "Conditional branch without target at 0x" + hexAddress(instruction.sourceAddress);
                return false;
            }
            std::uint64_t fallthrough = 0;
            for (const auto successor : block.successors)
                if (successor != instruction.target) { fallthrough = successor; break; }
            if (!fallthrough) {
                error = "Conditional branch has no fallthrough at 0x" + hexAddress(instruction.sourceAddress);
                return false;
            }
            const char* condition = instruction.opcode == ir::Opcode::BranchIfEqual
                ? "state.flags.zf" : "!state.flags.zf";
            out << "        if (" << condition << ") goto " << blockLabel(instruction.target) << ";\n"
                << "        goto " << blockLabel(fallthrough) << ";\n";
            return true;
        }

        case ir::Opcode::Push: {
            if (!requireOperands(instruction, 1, error)) return false;
            if (!emitRead(out, instruction.operands[0], "push_" + suffix, error)) return false;
            out << "        state.reg(pes17::ir::Register::ESP) -= 4u;\n"
                << "        if (!state.memory.write32(state.reg(pes17::ir::Register::ESP), push_"
                << suffix << ")) return false;\n";
            return true;
        }

        case ir::Opcode::Pop: {
            if (!requireOperands(instruction, 1, error)) return false;
            out << "        const auto pop_" << suffix
                << " = state.memory.read32(state.reg(pes17::ir::Register::ESP));\n"
                << "        if (!pop_" << suffix << ") return false;\n";
            if (!emitWrite(out, instruction.operands[0], "*pop_" + suffix, error)) return false;
            out << "        state.reg(pes17::ir::Register::ESP) += 4u;\n";
            return true;
        }

        case ir::Opcode::Return:
            out << "        return true;\n";
            return true;

        case ir::Opcode::CallDirect:
        case ir::Opcode::CallIndirect: {
            if (!instruction.hasTarget) {
                // Não bloqueia o build se for uma chamada indireta sem alvo resolvido
                out << "        // TODO: Call without target at 0x" << hexAddress(instruction.sourceAddress) << "\n";
                return true;
            }
            out << "        if (!pes17::execution::DispatchWin32Thunk(state, " << immediateExpr(instruction.target) << ")) {\n"
                << "            /* TODO: call direct recompiled function at " << hexAddress(instruction.target) << " */\n"
                << "        }\n";
            return true;
        }

        case ir::Opcode::Unsupported:
        default:
            error = "Unsupported opcode " + std::string(ir::opcodeName(instruction.opcode)) +
                " at 0x" + hexAddress(instruction.sourceAddress);
            return false;
    }
}

}  // namespace

GeneratedCode CppGenerator::generate(
    const ir::Function& function,
    const std::string& functionName,
    std::uint64_t startAddress) const {

    GeneratedCode result;
    if (function.blocks.empty()) {
        result.error = "Function has no IR blocks";
        return result;
    }

    const auto entry = startAddress ? startAddress : function.entry;
    auto reachable = reachableBlocks(function, entry, result.error);
    if (reachable.empty()) return result;

    std::ostringstream out;
    out << "#include <cstdint>\n"
        << "#include \"runtime/execution/machine_state.h\"\n"
        << "#include \"runtime/win32/win32_bridge.h\"\n\n"
        << "bool " << functionName
        << "(pes17::execution::MachineState& state) {\n"
        << "    goto " << blockLabel(entry) << ";\n\n";

    for (const auto& block : function.blocks) {
        if (!reachable.contains(block.start)) continue;
        out << blockLabel(block.start) << ": {\n";
        for (std::size_t i = 0; i < block.instructions.size(); ++i) {
            if (!emitInstruction(out, block, block.instructions[i], i, result.error))
                return result;
        }
        const auto& last = block.instructions.back();
        const bool terminates = last.opcode == ir::Opcode::Branch ||
            last.opcode == ir::Opcode::BranchIfEqual ||
            last.opcode == ir::Opcode::BranchIfNotEqual ||
            last.opcode == ir::Opcode::Return ||
            last.opcode == ir::Opcode::CallDirect ||
            last.opcode == ir::Opcode::CallIndirect;
            
        if (!terminates) {
            if (block.successors.size() != 1) {
                result.error = "Block 0x" + hexAddress(block.start) +
                    " has no explicit terminator and not exactly one successor";
                return result;
            }
            out << "        goto " << blockLabel(block.successors.front()) << ";\n";
        }
        out << "    }\n\n";
    }

    out << "    return false;\n}\n";
    result.success = true;
    result.source = out.str();
    return result;
}

}  // namespace pes17::codegen
