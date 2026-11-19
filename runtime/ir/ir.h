#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "operand.h"

namespace pes17::ir {

enum class Opcode {
    Unsupported,

    Push,
    Pop,

    Move,
    Address,
    Load,
    Store,

    Add,
    Subtract,

    Compare,

    CallDirect,
    CallIndirect,

    Branch,
    BranchIfEqual,
    BranchIfNotEqual,

    Return
};

enum class Flag : std::uint8_t {
    ZF = 1 << 0,
    CF = 1 << 1,
    SF = 1 << 2,
    OF = 1 << 3
};

struct Instruction {
    Opcode opcode = Opcode::Unsupported;

    std::uint8_t flagsRead = 0;
    std::uint8_t flagsWritten = 0;

    std::uint64_t sourceAddress = 0;

    std::vector<Operand> operands;

    bool hasTarget = false;
    std::uint64_t target = 0;

    std::string originalMnemonic;
    std::string originalOperands;
};

struct Block {
    std::uint64_t start = 0;

    std::vector<Instruction> instructions;
    std::vector<std::uint64_t> successors;
};

struct Function {
    std::uint64_t entry = 0;
    std::vector<Block> blocks;

    std::size_t unsupportedCount = 0;
};

const char* opcodeName(Opcode opcode);

}
