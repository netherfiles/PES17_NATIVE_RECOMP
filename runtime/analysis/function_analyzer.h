#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "basic_block_builder.h"
#include "function.h"

namespace pes17::analysis {

class FunctionAnalyzer {
public:
    explicit FunctionAnalyzer(
        const BasicBlockBuilder& builder);

    std::optional<Function> analyze(
        std::uint64_t entry,
        std::size_t maxBlocks = 1024) const;

private:
    const BasicBlockBuilder& builder_;
};

}
