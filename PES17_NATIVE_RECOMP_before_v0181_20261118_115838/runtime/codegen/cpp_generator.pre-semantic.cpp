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
