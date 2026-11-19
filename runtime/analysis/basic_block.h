#pragma once

#include <cstdint>
#include <vector>

#include "../x86/decoder.h"

namespace pes17::analysis {

struct BasicBlock {
    std::uint64_t start = 0;
    std::uint64_t end = 0;

    std::vector<x86::DecodedInstruction> instructions;
    std::vector<std::uint64_t> successors;
    std::vector<std::uint64_t> calls;
};

}
