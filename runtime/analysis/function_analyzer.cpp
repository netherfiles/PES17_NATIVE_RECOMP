#include "function_analyzer.h"

#include <algorithm>
#include <deque>
#include <unordered_set>

namespace pes17::analysis {

FunctionAnalyzer::FunctionAnalyzer(
    const BasicBlockBuilder& builder)
    : builder_(builder) {
}

std::optional<Function>
FunctionAnalyzer::analyze(
    std::uint64_t entry,
    std::size_t maxBlocks) const {

    Function function;
    function.entry = entry;

    std::deque<std::uint64_t> pending;
    std::unordered_set<std::uint64_t> visited;
    std::unordered_set<std::uint64_t> seenCalls;

    pending.push_back(entry);

    while (!pending.empty()) {
        if (function.blocks.size() >= maxBlocks)
            return std::nullopt;

        const auto address = pending.front();
        pending.pop_front();

        if (!visited.insert(address).second)
            continue;

        auto block = builder_.build(address);

        if (!block)
            return std::nullopt;

        for (const auto call : block->calls) {
            if (seenCalls.insert(call).second)
                function.calls.push_back(call);
        }

        for (const auto successor : block->successors) {
            if (!visited.contains(successor))
                pending.push_back(successor);
        }

        function.blocks.emplace(
            block->start,
            std::move(*block));
    }

    std::sort(
        function.calls.begin(),
        function.calls.end());

    return function;
}

}
