#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "../analysis/call_graph.h"
#include "../analysis/function_normalizer.h"
#include "../ir/verifier.h"
#include "../ir/x86_lifter.h"
#include "cpp_generator.h"

namespace pes17::codegen {

enum class BatchStatus {
    Generated,
    NormalizeFailed,
    VerifyFailed,
    CodegenFailed,
    WriteFailed
};

struct BatchFunctionResult {
    std::uint64_t address = 0;
    BatchStatus status = BatchStatus::CodegenFailed;
    std::size_t blocks = 0;
    std::size_t instructions = 0;
    std::size_t unsupportedInstructions = 0;
    std::string outputPath;
    std::string reason;
};

struct BatchReport {
    std::size_t analyzed = 0;
    std::size_t generated = 0;
    std::size_t blocked = 0;
    std::size_t unresolved = 0;
    std::vector<BatchFunctionResult> functions;
    std::map<std::string, std::size_t> blockers;
};

class BatchRecompiler {
public:
    BatchReport run(
        const analysis::CallGraph& graph,
        const std::string& outputDirectory) const;

    bool writeReport(
        const BatchReport& report,
        const std::string& path) const;

private:
    analysis::FunctionNormalizer normalizer_;
    ir::X86Lifter lifter_;
    ir::Verifier verifier_;
    CppGenerator generator_;
};

const char* batchStatusName(BatchStatus status);

}
