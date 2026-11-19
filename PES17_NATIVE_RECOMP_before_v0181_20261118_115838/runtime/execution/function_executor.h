#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../ir/ir.h"
#include "executor.h"

namespace pes17::execution {

struct FunctionExecutionResult {
    bool success = true;
    bool returned = false;

    std::size_t blocksExecuted = 0;
    std::size_t instructionsExecuted = 0;

    std::uint64_t lastBlock = 0;
    std::string error;
};

class FunctionExecutor {
public:
    explicit FunctionExecutor(MachineState& state);

    FunctionExecutionResult execute(
        const ir::Function& function,
        std::uint64_t startBlock,
        std::size_t maxBlocks = 1024);

private:
    const ir::Block* findBlock(
        const ir::Function& function,
        std::uint64_t address) const;

    Executor executor_;
};

}
