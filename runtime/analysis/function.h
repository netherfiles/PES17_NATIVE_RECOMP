#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "basic_block.h"

namespace pes17::analysis {

struct Function {
    std::uint64_t entry = 0;

    std::map<std::uint64_t, BasicBlock> blocks;
    std::vector<std::uint64_t> calls;
};

}
