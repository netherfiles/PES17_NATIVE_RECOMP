#include "cpp_generator.h"

#include <iomanip>
#include <sstream>

namespace pes17::codegen {

namespace {

std::string hexAddress(std::uint64_t address) {
    std::ostringstream out;

    out << std::hex
        << std::setw(8)
        << std::setfill('0')
        << address;

    return out.str();
}

std::string blockLabel(std::uint64_t address) {
    return "block_" + hexAddress(address);
}

std::string regExpr(pes17::ir::Register reg) {
    using R = pes17::ir::Register;

    switch (reg) {
        case R::EAX:
            return "state.reg(pes17::ir::Register::EAX)";
        case R::EBX:
            return "state.reg(pes17::ir::Register::EBX)";
        case R::ECX:
            return "state.reg(pes17::ir::Register::ECX)";
        case R::EDX:
            return "state.reg(pes17::ir::Register::EDX)";
        case R::ESI:
            return "state.reg(pes17::ir::Register::ESI)";
        case R::EDI:
            return "state.reg(pes17::ir::Register::EDI)";
        case R::EBP:
            return "state.reg(pes17::ir::Register::EBP)";
        case R::ESP:
            return "state.reg(pes17::ir::Register::ESP)";
        case R::None:
            break;
    }

    return {};
}

std::string immediateExpr(std::int64_t value) {
    std::ostringstream out;

    out << "0x"
        << std::hex
        << static_cast<std::uint32_t>(value)
        << "u";

    return out.str();
}

std::string addressExpr(
    const pes17::ir::MemoryOperand& memory) {

    std::string expression;

    if (memory.base != pes17::ir::Register::None) {
        expression = regExpr(memory.base);
    }

    if (memory.index != pes17::ir::Register::None) {
        if (!expression.empty())
            expression += " + ";

        expression += "(";
        expression += regExpr(memory.index);
        expression += " * ";
        expression += std::to_string(memory.scale);
        expression += "u)";
    }

    if (memory.displacement != 0) {
        if (!expression.empty())
            expression += " + ";

        expression += immediateExpr(
            memory.displacement);
    }

    if (expression.empty())
        expression = "0u";

    return "(" + expression + ")";
}

std::string readExpr(const pes17::ir::Operand& operand) {
    using K = pes17::ir::OperandKind;

    switch (operand.kind) {
        case K::Register:
            return regExpr(operand.reg);

        case K::Immediate:
            return immediateExpr(
                operand.immediate);

        case K::Memory:
            return "state.memory.read32(" +
                addressExpr(operand.memory) +
                ").value()";

        case K::None:
            break;
    }

    return {};
}

std::string writeStatement(
    const pes17::ir::Operand& destination,
    const std::string& value) {

    using K = pes17::ir::OperandKind;

    switch (destination.kind) {
        case K::Register:
            return regExpr(destination.reg) +
                " = " + value + ";";

        case K::Memory:
            return "if (!state.memory.write32(" +
                addressExpr(destination.memory) +
                ", " + value + ")) return false;";

        case K::Immediate:
        case K::None:
            break;
    }

    return {};
}


}
GeneratedCode CppGenerator::generate(
    const ir::Function& function,
    const std::string& functionName) const {

    GeneratedCode result;

    if (function.blocks.empty()) {
        result.error = "Function has no IR blocks";
        return result;
    }

    std::ostringstream out;

    out
        << "#include <cstdint>\n\n"
        << "struct Pes17GeneratedState {\n"
        << "    std::uint32_t eax = 0;\n"
        << "    std::uint32_t ebx = 0;\n"
        << "    std::uint32_t ecx = 0;\n"
        << "    std::uint32_t edx = 0;\n"
        << "    std::uint32_t esi = 0;\n"
        << "    std::uint32_t edi = 0;\n"
        << "    std::uint32_t ebp = 0;\n"
        << "    std::uint32_t esp = 0;\n"
        << "    bool zf = false;\n"
        << "};\n\n";

    out
        << "bool "
        << functionName
        << "(Pes17GeneratedState& state) {\n"
        << "    (void)state;\n"
        << "    goto "
        << blockLabel(function.entry)
        << ";\n\n";

    for (const auto& block : function.blocks) {
        out
            << blockLabel(block.start)
            << ":\n";

        for (const auto& instruction :
             block.instructions) {

            out
                << "    // 0x"
                << hexAddress(
                    instruction.sourceAddress)
                << " "
                << instruction.originalMnemonic;

            if (!instruction.originalOperands.empty()) {
                out
                    << " "
                    << instruction.originalOperands;
            }

            out << "\n";
        }

        out << "\n";
    }

    out
        << "    return true;\n"
        << "}\n";

    result.success = true;
    result.source = out.str();

    return result;
}

}
