#include "batch_recompiler.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace pes17::codegen {

namespace {

std::string hexAddress(std::uint64_t address) {
    std::ostringstream out;
    out << std::hex << std::setw(8) << std::setfill('0') << address;
    return out.str();
}

std::string functionName(std::uint64_t address) {
    return "pes17_fn_" + hexAddress(address);
}

std::string firstLine(const std::string& text) {
    const auto end = text.find('\n');
    return text.substr(0, end);
}

std::size_t instructionCount(const ir::Function& function) {
    std::size_t total = 0;
    for (const auto& block : function.blocks)
        total += block.instructions.size();
    return total;
}

void addBlocker(BatchReport& report, const std::string& reason) {
    ++report.blockers[firstLine(reason.empty() ? "Unknown" : reason)];
}

}

const char* batchStatusName(BatchStatus status) {
    switch (status) {
        case BatchStatus::Generated: return "generated";
        case BatchStatus::NormalizeFailed: return "normalize-failed";
        case BatchStatus::VerifyFailed: return "verify-failed";
        case BatchStatus::CodegenFailed: return "codegen-failed";
        case BatchStatus::WriteFailed: return "write-failed";
    }
    return "unknown";
}

BatchReport BatchRecompiler::run(
    const analysis::CallGraph& graph,
    const std::string& outputDirectory) const {

    BatchReport report;
    report.unresolved = graph.unresolved.size();

    std::error_code error;
    std::filesystem::create_directories(outputDirectory, error);

    for (const auto& [address, node] : graph.functions) {
        BatchFunctionResult result;
        result.address = address;
        result.blocks = node.function.blocks.size();
        ++report.analyzed;

        const auto normalized = normalizer_.normalize(node.function);
        if (!normalized) {
            result.status = BatchStatus::NormalizeFailed;
            result.reason = "Function normalization failed";
            ++report.blocked;
            addBlocker(report, result.reason);
            report.functions.push_back(std::move(result));
            continue;
        }

        const auto lifted = lifter_.lift(*normalized);
        result.blocks = lifted.blocks.size();
        result.instructions = instructionCount(lifted);
        result.unsupportedInstructions = lifted.unsupportedCount;

        if (lifted.unsupportedCount != 0) {
            result.status = BatchStatus::VerifyFailed;
            result.reason =
                "IR contains unsupported instructions: " +
                std::to_string(lifted.unsupportedCount);

            ++report.blocked;
            addBlocker(report, result.reason);

            for (const auto& block : lifted.blocks) {
                for (const auto& instruction :
                     block.instructions) {

                    if (instruction.opcode !=
                        ir::Opcode::Unsupported)
                        continue;

                    const auto key =
                        "Unsupported x86 " +
                        instruction.originalMnemonic;

                    ++report.blockers[key];

                    result.reason +=
                        " | 0x" +
                        hexAddress(
                            instruction.sourceAddress) +
                        " " +
                        instruction.originalMnemonic;

                    if (!instruction.originalOperands.empty())
                        result.reason +=
                            " " +
                            instruction.originalOperands;
                }
            }

            report.functions.push_back(
                std::move(result));

            continue;
        }

        const auto verification = verifier_.verify(lifted);
        if (!verification.valid) {
            result.status = BatchStatus::VerifyFailed;
            result.reason = verification.messages.empty()
                ? "IR verification failed"
                : verification.messages.front();
            ++report.blocked;
            addBlocker(report, result.reason);
            report.functions.push_back(std::move(result));
            continue;
        }

        const auto name = functionName(address);
        const auto generated = generator_.generate(lifted, name, address);
        if (!generated.success) {
            result.status = BatchStatus::CodegenFailed;
            result.reason = generated.error;
            ++report.blocked;
            addBlocker(report, result.reason);
            report.functions.push_back(std::move(result));
            continue;
        }

        const auto path = std::filesystem::path(outputDirectory) /
            (name + ".cpp");
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            result.status = BatchStatus::WriteFailed;
            result.reason = "Could not open generated output file";
            ++report.blocked;
            addBlocker(report, result.reason);
            report.functions.push_back(std::move(result));
            continue;
        }
        file << generated.source;
        if (!file) {
            result.status = BatchStatus::WriteFailed;
            result.reason = "Could not write generated output file";
            ++report.blocked;
            addBlocker(report, result.reason);
            report.functions.push_back(std::move(result));
            continue;
        }

        result.status = BatchStatus::Generated;
        result.outputPath = path.string();
        ++report.generated;
        report.functions.push_back(std::move(result));
    }

    return report;
}

bool BatchRecompiler::writeReport(
    const BatchReport& report,
    const std::string& path) const {

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out << "PES17_NATIVE_RECOMP V0.18 batch report\n"
        << "analyzed=" << report.analyzed << '\n'
        << "generated=" << report.generated << '\n'
        << "blocked=" << report.blocked << '\n'
        << "unresolved=" << report.unresolved << "\n\n";

    out << "blockers:\n";
    if (report.blockers.empty()) out << "  none\n";
    for (const auto& [reason, count] : report.blockers)
        out << "  " << count << " x " << reason << '\n';

    out << "\nfunctions:\n";
    for (const auto& function : report.functions) {
        out << "  0x" << hexAddress(function.address)
            << " status=" << batchStatusName(function.status)
            << " blocks=" << function.blocks
            << " instructions=" << function.instructions
            << " unsupported=" << function.unsupportedInstructions;
        if (!function.outputPath.empty())
            out << " output=" << function.outputPath;
        if (!function.reason.empty())
            out << " reason=" << function.reason;
        out << '\n';
    }

    return static_cast<bool>(out);
}

}
