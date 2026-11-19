#pragma once

#include "operand.h"
#include <cstdint>
#include <string>
#include <vector>

namespace pes17::ir {

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

struct VerificationResult {
    bool valid = true;
    int errors = 0;
    std::vector<std::string> messages;
};

} // namespace pes17::ir
