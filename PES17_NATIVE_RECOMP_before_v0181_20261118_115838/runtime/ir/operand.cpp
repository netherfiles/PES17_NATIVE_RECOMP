#include "operand.h"

#include <sstream>

namespace pes17::ir {

const char* registerName(Register reg) {
    switch (reg) {
        case Register::EAX: return "eax";
        case Register::EBX: return "ebx";
        case Register::ECX: return "ecx";
        case Register::EDX: return "edx";
        case Register::ESI: return "esi";
        case Register::EDI: return "edi";
        case Register::EBP: return "ebp";
        case Register::ESP: return "esp";
        case Register::None: return "none";
    }

    return "unknown";
}

std::string operandToString(const Operand& operand) {
    std::ostringstream out;

    switch (operand.kind) {
        case OperandKind::None:
            return "-";

        case OperandKind::Register:
            out << registerName(operand.reg);
            break;

        case OperandKind::Immediate:
            out << "0x"
                << std::hex
                << static_cast<std::uint64_t>(
                    operand.immediate);
            break;

        case OperandKind::Memory:
            out << '[';

            if (operand.memory.base != Register::None)
                out << registerName(operand.memory.base);

            if (operand.memory.index != Register::None) {
                if (operand.memory.base != Register::None)
                    out << '+';

                out << registerName(operand.memory.index);

                if (operand.memory.scale != 1)
                    out << '*'
                        << static_cast<unsigned>(
                            operand.memory.scale);
            }

            if (operand.memory.displacement != 0) {
                if (operand.memory.displacement > 0)
                    out << '+';

                out << operand.memory.displacement;
            }

            out << ']';
            break;
    }

    if (operand.width)
        out << ':' << static_cast<unsigned>(operand.width);

    return out.str();
}

}
