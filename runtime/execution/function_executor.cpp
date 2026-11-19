#include "function_executor.h"

namespace pes17::execution {

FunctionExecutor::FunctionExecutor(
    MachineState& state)
    : executor_(state) {
}

const ir::Block*
FunctionExecutor::findBlock(
    const ir::Function& function,
    std::uint64_t address) const {

    for (const auto& block : function.blocks) {
        if (block.start == address)
            return &block;
    }

    return nullptr;
}

FunctionExecutionResult
FunctionExecutor::execute(
    const ir::Function& function,
    std::uint64_t startBlock,
    std::size_t maxBlocks) {

    FunctionExecutionResult result;
    std::uint64_t current = startBlock;

    for (std::size_t step = 0;
         step < maxBlocks;
         ++step) {

        const auto* block =
            findBlock(function, current);

        if (!block) {
            result.success = false;
            result.error =
                "Target basic block not found";
            result.lastBlock = current;
            return result;
        }

        result.lastBlock = current;
        ++result.blocksExecuted;

        const auto blockResult =
            executor_.executeBlock(*block);

        result.instructionsExecuted +=
            blockResult.instructionsExecuted;

        if (!blockResult.success) {
            result.success = false;
            result.error = blockResult.error;
            return result;
        }

        if (blockResult.returned) {
            result.returned = true;
            return result;
        }

        if (blockResult.nextBlock == 0) {
            result.success = false;
            result.error =
                "Block finished without next target";
            return result;
        }

        current = blockResult.nextBlock;
    }

    result.success = false;
    result.error =
        "Function block execution limit reached";

    return result;
}

}
