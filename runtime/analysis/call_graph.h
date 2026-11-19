#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "function.h"

namespace pes17::analysis {

struct CallGraphNode {
    Function function;
    std::size_t depth = 0;
};

struct CallGraph {
    std::uint64_t root = 0;

    std::map<std::uint64_t, CallGraphNode> functions;
    std::vector<std::uint64_t> unresolved;
};

}
