#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>

#include "core/build_info.h"
#include "pe/pe_image.h"
#include "x86/decoder.h"
#include "analysis/basic_block_builder.h"
#include "analysis/function_analyzer.h"
#include "analysis/call_graph_analyzer.h"
#include "analysis/function_normalizer.h"
#include "ir/x86_lifter.h"
#include "ir/verifier.h"

// INCLUDES QUE FALTAVAM:
#include "execution/machine_state.h"
#include "execution/executor.h"
#include "execution/machine_state_test.h"
#include "execution/executor_test.h"
#include "execution/function_executor_test.h"
#include "execution/function_executor.h"
#include "codegen/cpp_generator.h"
#include "codegen/batch_recompiler.h"

int main(int argc, char** argv) {
    using pes17::BuildInfo;
    using pes17::PeImage;

    std::cout << "====================================\n";
    if (!pes17::execution::runMachineStateSelfTest()) return 20;
    if (!pes17::execution::runExecutorSelfTest()) return 21;
    if (!pes17::execution::runBranchSelfTest()) return 22;
    if (!pes17::execution::runStackSelfTest()) return 24;
    if (!pes17::execution::runFunctionExecutorSelfTest()) return 23;

    std::cout << " PES 2017 Native Recomp Runtime\n";
    std::cout << "====================================\n";
    std::cout << "Runtime : " << BuildInfo::kRuntimeVersion << '\n';
    std::cout << "Build   : " << BuildInfo::kTargetBuild << '\n';
    std::cout << "Platform: " << BuildInfo::platform() << '\n';
    std::cout << "Arch    : " << BuildInfo::architecture() << "\n\n";

    if (argc != 2) return 1;

    PeImage pe;
    if (!pe.load(argv[1])) return 2;

    std::cout << "PE validation: OK\n";
    std::cout << "Machine       : 0x" << std::hex << pe.machine() << '\n';
    std::cout << "ImageBase     : 0x" << pe.imageBase() << '\n';
    std::cout << "EntryPoint RVA: 0x" << pe.entryPointRva() << '\n';
    std::cout << std::dec << "Sections      : " << pe.sectionCount() << "\n\n";

    if (pe.machine() != 0x014c) return 3;

    pes17::x86::Decoder decoder;
    if (!decoder.valid()) return 6;

    pes17::analysis::BasicBlockBuilder blockBuilder(pe, decoder);
    const std::uint64_t entryVa = static_cast<std::uint64_t>(pe.imageBase()) + pe.entryPointRva();
    const auto entryBlock = blockBuilder.build(entryVa);
    if (!entryBlock) return 8;

    pes17::analysis::FunctionAnalyzer functionAnalyzer(blockBuilder);
    const auto function = functionAnalyzer.analyze(entryVa, 1024);
    if (!function) return 9;

    pes17::analysis::CallGraphAnalyzer callGraphAnalyzer(pe, functionAnalyzer);
    const auto callGraph = callGraphAnalyzer.analyze(entryVa, 1, 8);

    std::cout << "\nV0.18 batch recompilation:\n";
    const std::string batchOutputDirectory = "/storage/emulated/0/Download/PES17_NATIVE_RECOMP/generated/batch";
    const std::string batchReportPath = "/storage/emulated/0/Download/PES17_NATIVE_RECOMP/generated/batch_report.txt";

    pes17::codegen::BatchRecompiler batchRecompiler;
    const auto batchReport = batchRecompiler.run(callGraph, batchOutputDirectory);
    if (!batchRecompiler.writeReport(batchReport, batchReportPath)) return 60;

    std::cout << "Analyzed  : " << batchReport.analyzed << '\n'
              << "Generated : " << batchReport.generated << '\n'
              << "Blocked   : " << batchReport.blocked << '\n'
              << "Unresolved: " << batchReport.unresolved << '\n';

    for (const auto& [reason, count] : batchReport.blockers) {
        std::cout << "  blocker " << count << " x " << reason << '\n';
    }

    constexpr std::uint64_t candidateAddress = 0x047d5000ULL;
    const auto candidate = functionAnalyzer.analyze(candidateAddress, 128);
    if (!candidate) return 10;

    pes17::analysis::FunctionNormalizer normalizer;
    const auto normalized = normalizer.normalize(*candidate);
    if (!normalized) return 11;

    pes17::ir::X86Lifter lifter;
    const auto lifted = lifter.lift(*normalized);

    std::cout << "\nReal PES IR test:\n";
    constexpr std::uint64_t realBlockAddress = 0x047d5024ULL;
    constexpr std::uint64_t expectedTarget   = 0x047d5037ULL;

    const pes17::ir::Block* realBlock = nullptr;
    for (const auto& block : lifted.blocks) {
        if (block.start == realBlockAddress) {
            realBlock = &block;
            break;
        }
    }
    if (!realBlock) return 30;

    constexpr std::uint32_t guestBase   = 0x10000000u;
    constexpr std::uint32_t listAddress = guestBase + 0x100u;

    // NOME MUDADO PARA EVITAR CONFLITO COM "realpath"
    pes17::execution::MachineState testState(guestBase, 0x1000);
    testState.reg(pes17::ir::Register::ECX) = listAddress;
    if (!testState.memory.write32(listAddress, 0)) return 31;

    pes17::execution::Executor testExecutor(testState);
    const auto realResult = testExecutor.executeBlock(*realBlock);
    if (!realResult.success) return 32;
    if (!testState.flags.zf) return 33;
    if (realResult.nextBlock != expectedTarget) return 34;

    std::cout << "Real PES IR test: PASS\n";

    std::cout << "\nReal PES loop test:\n";
    constexpr std::uint64_t loopStart = 0x047d5024ULL;
    constexpr std::uint32_t loopGuestBase = 0x10000000u;
    constexpr std::uint32_t loopListAddress = loopGuestBase + 0x100u;
    constexpr std::uint32_t loopTargetAddress = loopGuestBase + 0x300u;
    constexpr std::uint32_t loopStackAddress = loopGuestBase + 0x800u;
    constexpr std::uint32_t loopEaxValue = 5u;
    constexpr std::uint32_t loopAddValue = 7u;
    constexpr std::uint32_t oldEbpValue = 0x55667788u;

    // NOME MUDADO PARA EVITAR CONFLITO
    pes17::execution::MachineState loopTestState(loopGuestBase, 0x1000);
    loopTestState.reg(pes17::ir::Register::EAX) = loopEaxValue;
    loopTestState.reg(pes17::ir::Register::ECX) = loopListAddress;
    loopTestState.reg(pes17::ir::Register::EBP) = loopStackAddress;

    bool loopMemoryReady = true;
    loopMemoryReady &= loopTestState.memory.write32(loopListAddress, loopTargetAddress);
    loopMemoryReady &= loopTestState.memory.write32(loopListAddress + 4, loopAddValue);
    loopMemoryReady &= loopTestState.memory.write32(loopListAddress + 8, 0);
    loopMemoryReady &= loopTestState.memory.write32(loopTargetAddress, 0);
    loopMemoryReady &= loopTestState.memory.write32(loopStackAddress, oldEbpValue);
    if (!loopMemoryReady) return 35;

    pes17::execution::FunctionExecutor loopExecutor(loopTestState);
    const auto loopResult = loopExecutor.execute(lifted, loopStart, 16);
    if (!loopResult.success) return 36;
    if (!loopResult.returned) return 37;

    std::cout << "Real PES loop test: PASS\n";

    std::cout << "\nC++ codegen test:\n";
    pes17::codegen::CppGenerator cppGenerator;
    const auto generatedCode = cppGenerator.generate(lifted, "pes17_fn_047d5000", 0x047d5024ULL);
    if (!generatedCode.success) return 50;

    const char* generatedPath = "/storage/emulated/0/Download/PES17_NATIVE_RECOMP/generated/functions/pes17_fn_047d5000.cpp";
    std::ofstream generatedFile(generatedPath, std::ios::binary);
    if (!generatedFile) return 51;

    generatedFile << generatedCode.source;
    generatedFile.close();

    std::cout << "C++ codegen: PASS\n";

    return 0;
}
