#include "function_normalizer.h"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

namespace pes17::analysis {

std::optional<Function>
FunctionNormalizer::normalize(
    const Function& function) const {

    if (function.blocks.empty())
        return std::nullopt;

    std::map<
        std::uint64_t,
        x86::DecodedInstruction> instructionMap;

    std::set<std::uint64_t> leaders;
    leaders.insert(function.entry);

    for (const auto& [blockAddress, block] :
         function.blocks) {

        leaders.insert(blockAddress);

        for (const auto& instruction :
             block.instructions) {

            instructionMap.emplace(
                instruction.address,
                instruction);
        }

        for (const auto successor :
             block.successors) {

            leaders.insert(successor);
        }
    }

    if (!instructionMap.contains(function.entry))
        return std::nullopt;

    Function normalized;
    normalized.entry = function.entry;

    std::unordered_set<std::uint64_t> seenCalls;

    for (const auto leader : leaders) {
        auto iterator =
            instructionMap.lower_bound(leader);

        if (iterator == instructionMap.end() ||
            iterator->first != leader) {
            continue;
        }

        BasicBlock block;
        block.start = leader;

        while (iterator != instructionMap.end()) {
            const auto currentAddress =
                iterator->first;

            if (currentAddress != leader &&
                leaders.contains(currentAddress)) {
                block.successors.push_back(
                    currentAddress);
                break;
            }

            const auto instruction =
                iterator->second;

            if (!block.instructions.empty()) {
                const auto& previous =
                    block.instructions.back();

                const auto expectedAddress =
                    previous.address +
                    previous.size;

                if (currentAddress != expectedAddress)
                    break;
            }

            block.instructions.push_back(
                instruction);

            block.end =
                instruction.address +
                instruction.size;

            if (instruction.flow ==
                    x86::FlowType::Call &&
                instruction.hasTarget) {

                block.calls.push_back(
                    instruction.target);

                if (seenCalls.insert(
                        instruction.target).second) {

                    normalized.calls.push_back(
                        instruction.target);
                }
            }

            if (instruction.flow ==
                x86::FlowType::ConditionalJump) {

                if (instruction.hasTarget) {
                    block.successors.push_back(
                        instruction.target);
                }

                block.successors.push_back(
                    instruction.address +
                    instruction.size);

                break;
            }

            if (instruction.flow ==
                x86::FlowType::Jump) {

                if (instruction.hasTarget) {
                    block.successors.push_back(
                        instruction.target);
                }

                break;
            }

            if (instruction.flow ==
                x86::FlowType::Return) {
                break;
            }

            ++iterator;
        }

        if (block.instructions.empty())
            continue;

        std::sort(
            block.successors.begin(),
            block.successors.end());

        block.successors.erase(
            std::unique(
                block.successors.begin(),
                block.successors.end()),
            block.successors.end());

        normalized.blocks.emplace(
            block.start,
            std::move(block));
    }

    std::sort(
        normalized.calls.begin(),
        normalized.calls.end());

    return normalized;
}

}
