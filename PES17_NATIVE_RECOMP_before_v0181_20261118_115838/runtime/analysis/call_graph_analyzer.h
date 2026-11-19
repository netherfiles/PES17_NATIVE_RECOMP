#pragma once

#include <cstddef>
#include <cstdint>

#include "call_graph.h"
#include "function_analyzer.h"
#include "../pe/pe_image.h"

namespace pes17::analysis {

class CallGraphAnalyzer {
public:
    CallGraphAnalyzer(
        const PeImage& image,
        const FunctionAnalyzer& analyzer);

    CallGraph analyze(
        std::uint64_t root,
        std::size_t maxDepth = 1,
        std::size_t maxFunctions = 32) const;

private:
    bool isMapped(std::uint64_t address) const;

    const PeImage& image_;
    const FunctionAnalyzer& analyzer_;
};

}
