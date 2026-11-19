#pragma once

#include <cstdint>
#include <string>

namespace pes17::ir {

enum class OperandKind {
    None,
    Register,
    Immediate,
    Memory
};

enum class Register {
    None,
    EAX,
    EBX,
    ECX,
    EDX,
    ESI,
    EDI,
    EBP,
    ESP
};

struct MemoryOperand {
    Register base = Register::None;
    Register index = Register::None;
    std::uint8_t scale = 1;
    std::int64_t displacement = 0;
    std::uint8_t width = 0;
};

struct Operand {
    OperandKind kind = OperandKind::None;

    Register reg = Register::None;
    std::int64_t immediate = 0;
    MemoryOperand memory;

    std::uint8_t width = 0;
};

const char* registerName(Register reg);
std::string operandToString(const Operand& operand);

}
