#pragma once

#include "operand.h"
#include <cstdint>
#include <string>
#include <vector>

namespace pes17::ir {

enum class Flag : std::uint8_t {
    ZF = 1 << 0,
    CF = 1 << 1,
    SF = 1 << 2,
    OF = 1 << 3
};

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
    Fcmovu,
    Unsupported
};

const char* opcodeName(Opcode opcode);

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

} // namespace pes17::ir
