#include "call_graph_analyzer.h"

#include <deque>
#include <unordered_set>
#include <utility>

namespace pes17::analysis {

namespace {

struct PendingFunction {
    std::uint64_t address = 0;
    std::size_t depth = 0;
};

}

CallGraphAnalyzer::CallGraphAnalyzer(
    const PeImage& image,
    const FunctionAnalyzer& analyzer)
    : image_(image),
      analyzer_(analyzer) {
}

bool CallGraphAnalyzer::isMapped(
    std::uint64_t address) const {

    if (address < image_.imageBase())
        return false;

    const auto rva64 =
        address - image_.imageBase();

    if (rva64 > 0xffffffffULL)
        return false;

    const auto rva =
        static_cast<std::uint32_t>(rva64);

    return image_.sectionForRva(rva) != nullptr &&
           image_.rvaToFileOffset(rva).has_value();
}

CallGraph CallGraphAnalyzer::analyze(
    std::uint64_t root,
    std::size_t maxDepth,
    std::size_t maxFunctions) const {

    CallGraph graph;
    graph.root = root;

    std::deque<PendingFunction> pending;
    std::unordered_set<std::uint64_t> queued;
    std::unordered_set<std::uint64_t> unresolved;

    pending.push_back({root, 0});
    queued.insert(root);

    while (!pending.empty() &&
           graph.functions.size() < maxFunctions) {

        const auto current = pending.front();
        pending.pop_front();

        if (!isMapped(current.address)) {
            if (unresolved.insert(current.address).second)
                graph.unresolved.push_back(current.address);
            continue;
        }

        auto function =
            analyzer_.analyze(
                current.address,
                1024);

        if (!function) {
            if (unresolved.insert(current.address).second)
                graph.unresolved.push_back(current.address);
            continue;
        }

        const auto calls = function->calls;

        CallGraphNode node;
        node.function = std::move(*function);
        node.depth = current.depth;

        graph.functions.emplace(
            current.address,
            std::move(node));

        if (current.depth >= maxDepth)
            continue;

        for (const auto target : calls) {
            if (!isMapped(target)) {
                if (unresolved.insert(target).second)
                    graph.unresolved.push_back(target);
                continue;
            }

            if (graph.functions.contains(target))
                continue;

            if (!queued.insert(target).second)
                continue;

            if (graph.functions.size() +
                    pending.size() >= maxFunctions)
                break;

            pending.push_back({
                target,
                current.depth + 1
            });
        }
    }

    return graph;
}

}
