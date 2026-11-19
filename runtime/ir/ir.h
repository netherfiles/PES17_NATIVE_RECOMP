#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pes17::ir {

enum class Register {
    None,
    EAX, EBX, ECX, EDX,
    ESI, EDI, EBP, ESP
};

enum class Flag : std::uint8_t {
    ZF = 1 << 0,
    CF = 1 << 1,
    SF = 1 << 2,
    OF = 1 << 3
};

enum class OperandKind {
    None,
    Register,
    Immediate,
    Memory
};

struct MemoryOperand {
    Register base = Register::None;
    Register index = Register::None;
    std::uint8_t scale = 0;
    std::int32_t displacement = 0;
    std::uint8_t width = 0;
};

struct Operand {
    OperandKind kind = OperandKind::None;
    std::uint8_t width = 0;
    Register reg = Register::None;
    std::uint64_t immediate = 0;
    MemoryOperand memory;
};

// AQUI ESTÃO TODAS AS INSTRUÇÕES QUE O SEU TRADUTOR CONHECE
enum class Opcode {
    Push,
    Pop,
    Load,
    Store,
    Move,
    Address,
    Add,
    Subtract,
    Compare,
    CallDirect,
    CallIndirect,
    Branch,
    BranchIfEqual,
    BranchIfNotEqual,
    Return,
    
    // As novas instruções que adicionamos para destravar o jogo
    Increment,
    Xor,
    ShiftLeft,
    Test,
    MoveZeroExtend,
    BranchIfAbove,
    SetIfEqual,
    SubtractWithBorrow,
    Exchange,
    PushFlags,

    Unsupported
};

struct Instruction {
    std::uint64_t sourceAddress = 0;
    std::string originalMnemonic;
    std::string originalOperands;
    bool hasTarget = false;
    std::uint64_t target = 0;
    std::vector<Operand> operands;
    Opcode opcode = Opcode::Unsupported;
    std::uint8_t flagsWritten = 0;
    std::uint8_t flagsRead = 0;
};

struct Block {
    std::uint64_t start = 0;
    std::vector<Instruction> instructions;
    std::vector<std::uint64_t> successors;
};

struct Function {
    std::uint64_t entry = 0;
    std::vector<Block> blocks;
    int unsupportedCount = 0;
};

struct VerificationResult {
    bool valid = true;
    int errors = 0;
    std::vector<std::string> messages;
};

} // namespace pes17::ir
