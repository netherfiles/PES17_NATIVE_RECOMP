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

// INCLUINDO OS ARQUIVOS QUE ESTAVAM FALTANDO:
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

    if (!pes17::execution::runMachineStateSelfTest()) {
        std::cerr << "MachineState self-test FAILED\n";
        return 20;
    }

    if (!pes17::execution::runExecutorSelfTest()) {
        std::cerr << "Executor self-test FAILED\n";
        return 21;
    }

    if (!pes17::execution::runBranchSelfTest()) {
        std::cerr << "Branch self-test FAILED\n";
        return 22;
    }

    if (!pes17::execution::runStackSelfTest()) {
        std::cerr << "Stack self-test FAILED\n";
        return 24;
    }

    if (!pes17::execution::runFunctionExecutorSelfTest()) {
        std::cerr << "FunctionExecutor self-test FAILED\n";
        return 23;
    }
    std::cout << " PES 2017 Native Recomp Runtime\n";
    std::cout << "====================================\n";
    std::cout << "Runtime : " << BuildInfo::kRuntimeVersion << '\n';
    std::cout << "Build   : " << BuildInfo::kTargetBuild << '\n';
    std::cout << "Platform: " << BuildInfo::platform() << '\n';
    std::cout << "Arch    : " << BuildInfo::architecture() << "\n\n";

    if (argc != 2) {
        std::cout << "Usage: pes17_host /path/to/PES2017.exe\n";
        return 1;
    }

    PeImage pe;

    if (!pe.load(argv[1])) {
        std::cerr << "PE load failed: " << pe.error() << '\n';
        return 2;
    }

    std::cout << "PE validation: OK\n";

    std::cout << "Machine       : 0x"
              << std::hex << pe.machine() << '\n';

    std::cout << "Optional magic: 0x"
              << pe.optionalMagic() << '\n';

    std::cout << "ImageBase     : 0x"
              << pe.imageBase() << '\n';

    std::cout << "EntryPoint RVA: 0x"
              << pe.entryPointRva() << '\n';

    std::cout << std::dec;
    std::cout << "Sections      : "
              << pe.sectionCount() << "\n\n";

    std::cout << "Section map:\n";

    for (const auto& s : pe.sections()) {
        std::cout
            << "  "
            << std::left << std::setw(9) << s.name
            << " RVA=0x" << std::right << std::hex
            << std::setw(8) << std::setfill('0')
            << s.virtualAddress
            << " VSZ=0x" << std::setw(8)
            << s.virtualSize
            << " RAW=0x" << std::setw(8)
            << s.rawOffset
            << " RSZ=0x" << std::setw(8)
            << s.rawSize
            << (s.executable() ? "  EXEC" : "")
            << '\n';

        std::cout << std::setfill(' ');
    }

    std::cout << std::dec;

    if (pe.machine() != 0x014c) {
        std::cerr << "\nUnsupported machine. Expected i386.\n";
        return 3;
    }

    std::cout << "\nTarget machine: x86/i386 confirmed\n";
    std::cout << "PE32 target accepted for recomp analysis\n";

    std::cout << "\nEntry point analysis:\n";

    const auto epRva = pe.entryPointRva();
    const auto* epSection = pe.sectionForRva(epRva);
    const auto epOffset = pe.rvaToFileOffset(epRva);

    if (!epSection) {
        std::cerr << "Entry point section: NOT FOUND\n";
        return 4;
    }

    std::cout << "Section        : "
              << epSection->name << '\n';

    if (!epOffset) {
        std::cerr << "Entry point file offset: NOT MAPPABLE\n";
        return 5;
    }

    std::cout << "File offset    : 0x"
              << std::hex << *epOffset << '\n';

    std::cout << "Virtual address: 0x"
              << (pe.imageBase() + epRva) << '\n';

    const auto bytes = pe.readBytesAtRva(epRva, 32);

    std::cout << "First 32 bytes :";

    for (const auto byte : bytes) {
        std::cout
            << ' '
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned>(byte);
    }

    std::cout << std::setfill(' ')
              << std::dec
              << '\n';

    std::cout << "\nEntry point disassembly:\n";

    pes17::x86::Decoder decoder;

    if (!decoder.valid()) {
        std::cerr << "Capstone x86 decoder initialization failed\n";
        return 6;
    }

    const auto code =
        pe.readBytesAtRva(pe.entryPointRva(), 256);

    const auto instructions = decoder.decode(
        code.data(),
        code.size(),
        static_cast<std::uint64_t>(
            pe.imageBase()) + pe.entryPointRva(),
        32);

    if (instructions.empty()) {
        std::cerr << "No x86 instructions decoded\n";
        return 7;
    }

    for (const auto& insn : instructions) {
        std::cout
            << "0x"
            << std::hex
            << std::right
            << std::setw(8)
            << std::setfill('0')
            << insn.address
            << "  "
            << std::left
            << std::setw(8)
            << std::setfill(' ')
            << insn.mnemonic
            << " "
            << std::setw(24)
            << insn.operands
            << "  ["
            << pes17::x86::flowName(insn.flow)
            << "]";

        if (insn.hasTarget) {
            std::cout
                << " -> 0x"
                << std::hex
                << insn.target;
        }

        std::cout << '\n';

        if (insn.flow == pes17::x86::FlowType::Return ||
            insn.flow == pes17::x86::FlowType::Jump) {
            break;
        }
    }

    std::cout << std::right << std::dec;

    std::cout << "\nBasic block analysis:\n";

    pes17::analysis::BasicBlockBuilder blockBuilder(
        pe,
        decoder);

    const std::uint64_t entryVa =
        static_cast<std::uint64_t>(pe.imageBase()) +
        pe.entryPointRva();

    const auto entryBlock =
        blockBuilder.build(entryVa);

    if (!entryBlock) {
        std::cerr << "Could not build entry basic block\n";
        return 8;
    }

    std::cout
        << "Start        : 0x"
        << std::hex
        << entryBlock->start
        << '\n';

    std::cout
        << "End          : 0x"
        << entryBlock->end
        << '\n';

    std::cout
        << std::dec
        << "Instructions : "
        << entryBlock->instructions.size()
        << '\n';

    std::cout
        << "Successors   : "
        << entryBlock->successors.size()
        << '\n';

    for (const auto successor : entryBlock->successors) {
        std::cout
            << "  successor -> 0x"
            << std::hex
            << successor
            << '\n';
    }

    std::cout
        << std::dec
        << "Calls        : "
        << entryBlock->calls.size()
        << '\n';

    for (const auto call : entryBlock->calls) {
        std::cout
            << "  call -> 0x"
            << std::hex
            << call
            << '\n';
    }

    std::cout << std::dec;

    std::cout << "\nFunction analysis:\n";

    pes17::analysis::FunctionAnalyzer functionAnalyzer(
        blockBuilder);

    const auto function =
        functionAnalyzer.analyze(
            entryVa,
            1024);

    if (!function) {
        std::cerr << "Could not analyze entry function\n";
        return 9;
    }

    std::cout
        << "Entry        : 0x"
        << std::hex
        << function->entry
        << '\n';

    std::cout
        << std::dec
        << "Blocks       : "
        << function->blocks.size()
        << '\n';

    std::size_t instructionCount = 0;

    for (const auto& [address, block] : function->blocks) {
        instructionCount += block.instructions.size();

        std::cout
            << "  block 0x"
            << std::hex
            << address
            << " -> 0x"
            << block.end
            << std::dec
            << "  insns="
            << block.instructions.size()
            << "  successors="
            << block.successors.size()
            << '\n';
    }

    std::cout
        << "Instructions : "
        << instructionCount
        << '\n';

    std::cout
        << "Calls        : "
        << function->calls.size()
        << '\n';

    for (const auto call : function->calls) {
        std::cout
            << "  function call -> 0x"
            << std::hex
            << call
            << '\n';
    }

    std::cout << std::dec;

    std::cout << "\nCall graph analysis:\n";

    pes17::analysis::CallGraphAnalyzer callGraphAnalyzer(
        pe,
        functionAnalyzer);

    const auto callGraph =
        callGraphAnalyzer.analyze(
            entryVa,
            1,
            8);

    std::cout
        << "Root        : 0x"
        << std::hex
        << callGraph.root
        << '\n';

    std::cout
        << std::dec
        << "Functions   : "
        << callGraph.functions.size()
        << '\n';

    std::size_t totalBlocks = 0;
    std::size_t totalInstructions = 0;

    for (const auto& [address, node] :
         callGraph.functions) {

        totalBlocks +=
            node.function.blocks.size();

        for (const auto& [blockAddress, block] :
             node.function.blocks) {
            (void)blockAddress;

            totalInstructions +=
                block.instructions.size();
        }

        std::cout
            << "  function 0x"
            << std::hex
            << address
            << std::dec
            << " depth="
            << node.depth
            << " blocks="
            << node.function.blocks.size()
            << " calls="
            << node.function.calls.size()
            << '\n';
    }

    std::cout
        << "Total blocks: "
        << totalBlocks
        << '\n';

    std::cout
        << "Total insns : "
        << totalInstructions
        << '\n';

    std::cout
        << "Unresolved  : "
        << callGraph.unresolved.size()
        << '\n';

    for (const auto address :
         callGraph.unresolved) {
        std::cout
            << "  unresolved -> 0x"
            << std::hex
            << address
            << '\n';
    }

    std::cout << std::dec;

    std::cout << "\nV0.18 batch recompilation:\n";

    const std::string batchOutputDirectory =
        "/storage/emulated/0/Download/PES17_NATIVE_RECOMP/"
        "generated/batch";

    const std::string batchReportPath =
        "/storage/emulated/0/Download/PES17_NATIVE_RECOMP/"
        "generated/batch_report.txt";

    pes17::codegen::BatchRecompiler batchRecompiler;
    const auto batchReport = batchRecompiler.run(
        callGraph,
        batchOutputDirectory);

    if (!batchRecompiler.writeReport(
            batchReport,
            batchReportPath)) {
        std::cerr << "Batch report write FAILED\n";
        return 60;
    }

    std::cout
        << "Analyzed  : " << batchReport.analyzed << '\n'
        << "Generated : " << batchReport.generated << '\n'
        << "Blocked   : " << batchReport.blocked << '\n'
        << "Unresolved: " << batchReport.unresolved << '\n'
        << "Report    : " << batchReportPath << '\n';

    for (const auto& [reason, count] : batchReport.blockers) {
        std::cout
            << "  blocker " << count
            << " x " << reason << '\n';
    }

    constexpr std::uint64_t candidateAddress =
        0x047d5000ULL;

    std::cout
        << "\nCandidate function dump: 0x"
        << std::hex
        << candidateAddress
        << '\n';

    const auto candidate =
        functionAnalyzer.analyze(
            candidateAddress,
            128);

    if (!candidate) {
        std::cerr << "Could not analyze candidate function\n";
        return 10;
    }

    for (const auto& [blockAddress, block] :
         candidate->blocks) {

        std::cout
            << "\nBLOCK 0x"
            << std::hex
            << blockAddress
            << " -> 0x"
            << block.end
            << '\n';

        for (const auto& insn : block.instructions) {
            std::cout
                << "  0x"
                << std::right
                << std::setw(8)
                << std::setfill('0')
                << std::hex
                << insn.address
                << "  "
                << std::left
                << std::setw(8)
                << std::setfill(' ')
                << insn.mnemonic
                << " "
                << insn.operands
                << "  ["
                << pes17::x86::flowName(insn.flow)
                << "]";

            if (insn.hasTarget) {
                std::cout
                    << " -> 0x"
                    << std::hex
                    << insn.target;
            }

            std::cout << '\n';
        }

        std::cout
            << "  successors:";

        if (block.successors.empty()) {
            std::cout << " none";
        } else {
            for (const auto successor :
                 block.successors) {
                std::cout
                    << " 0x"
                    << std::hex
                    << successor;
            }
        }

        std::cout << '\n';
    }

    std::cout << std::right << std::dec;

    std::cout << "\nNormalized function analysis:\n";

    pes17::analysis::FunctionNormalizer normalizer;

    const auto normalized =
        normalizer.normalize(*candidate);

    if (!normalized) {
        std::cerr << "Could not normalize candidate function\n";
        return 11;
    }

    std::set<std::uint64_t> uniqueInstructions;
    std::size_t duplicateInstructions = 0;
    std::size_t normalizedInstructionCount = 0;

    for (const auto& [blockAddress, block] :
         normalized->blocks) {

        std::cout
            << "\nNORMALIZED BLOCK 0x"
            << std::hex
            << blockAddress
            << " -> 0x"
            << block.end
            << std::dec
            << "  insns="
            << block.instructions.size()
            << '\n';

        for (const auto& insn :
             block.instructions) {

            ++normalizedInstructionCount;

            if (!uniqueInstructions.insert(
                    insn.address).second) {
                ++duplicateInstructions;
            }

            std::cout
                << "  0x"
                << std::right
                << std::setw(8)
                << std::setfill('0')
                << std::hex
                << insn.address
                << "  "
                << std::left
                << std::setw(8)
                << std::setfill(' ')
                << insn.mnemonic
                << " "
                << insn.operands
                << "  ["
                << pes17::x86::flowName(insn.flow)
                << "]";

            if (insn.hasTarget) {
                std::cout
                    << " -> 0x"
                    << std::hex
                    << insn.target;
            }

            std::cout << '\n';
        }

        std::cout << "  successors:";

        if (block.successors.empty()) {
            std::cout << " none";
        } else {
            for (const auto successor :
                 block.successors) {
                std::cout
                    << " 0x"
                    << std::hex
                    << successor;
            }
        }

        std::cout << '\n';
    }

    std::cout
        << "\nNormalized summary:\n"
        << "Blocks       : "
        << std::dec
        << normalized->blocks.size()
        << '\n'
        << "Instructions : "
        << normalizedInstructionCount
        << '\n'
        << "Unique insns : "
        << uniqueInstructions.size()
        << '\n'
        << "Duplicates   : "
        << duplicateInstructions
        << '\n';

    std::cout << std::right << std::dec;

    std::cout << "\nFirst IR lift:\n";

    pes17::ir::X86Lifter lifter;
    const auto lifted =
        lifter.lift(*normalized);

    std::size_t irInstructionCount = 0;

    std::cout
        << "Entry       : 0x"
        << std::hex
        << lifted.entry
        << '\n';

    for (const auto& block : lifted.blocks) {
        std::cout
            << "\nIR BLOCK 0x"
            << std::hex
            << block.start
            << '\n';

        for (const auto& instruction :
             block.instructions) {

            ++irInstructionCount;

            std::cout
                << "  0x"
                << std::right
                << std::setw(8)
                << std::setfill('0')
                << std::hex
                << instruction.sourceAddress
                << "  "
                << std::left
                << std::setw(18)
                << std::setfill(' ')
                << pes17::ir::opcodeName(
                    instruction.opcode)
                << "  "
                << instruction.originalMnemonic;

            if (!instruction.originalOperands.empty()) {
                std::cout
                    << " "
                    << instruction.originalOperands;
            }

            if (!instruction.operands.empty()) {
                std::cout << "  {";

                for (std::size_t i = 0;
                     i < instruction.operands.size();
                     ++i) {

                    if (i != 0)
                        std::cout << ", ";

                    std::cout
                        << pes17::ir::operandToString(
                            instruction.operands[i]);
                }

                std::cout << "}";
            }

            if (instruction.hasTarget) {
                std::cout
                    << " -> 0x"
                    << std::hex
                    << instruction.target;
            }

            std::cout << '\n';
        }

        std::cout << "  successors:";

        if (block.successors.empty()) {
            std::cout << " none";
        } else {
            for (const auto successor :
                 block.successors) {
                std::cout
                    << " 0x"
                    << std::hex
                    << successor;
            }
        }

        std::cout << '\n';
    }

    std::cout
        << "\nIR summary:\n"
        << std::dec
        << "Blocks       : "
        << lifted.blocks.size()
        << '\n'
        << "Instructions : "
        << irInstructionCount
        << '\n'
        << "Unsupported  : "
        << lifted.unsupportedCount
        << '\n';

    std::cout << std::right << std::dec;

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

    if (!realBlock) {
        std::cerr << "Real PES IR test FAILED: block not found\n";
        return 30;
    }

    constexpr std::uint32_t guestBase   = 0x10000000u;
    constexpr std::uint32_t listAddress = guestBase + 0x100u;

    pes17::execution::MachineState realState(
        guestBase,
        0x1000);

    realState.reg(pes17::ir::Register::ECX) =
        listAddress;

    if (!realState.memory.write32(listAddress, 0)) {
        std::cerr << "Real PES IR test FAILED: memory setup\n";
        return 31;
    }

    pes17::execution::Executor realExecutor(realState);

    const auto realResult =
        realExecutor.executeBlock(*realBlock);

    if (!realResult.success) {
        std::cerr
            << "Real PES IR test FAILED: "
            << realResult.error
            << '\n';
        return 32;
    }

    if (!realState.flags.zf) {
        std::cerr << "Real PES IR test FAILED: ZF\n";
        return 33;
    }

    if (realResult.nextBlock != expectedTarget) {
        std::cerr
            << "Real PES IR test FAILED: target 0x"
            << std::hex
            << realResult.nextBlock
            << '\n';
        return 34;
    }

    std::cout
        << "Real PES IR test: PASS\n"
        << "  Source block : 0x"
        << std::hex << realBlockAddress << '\n'
        << "  ZF           : 1\n"
        << "  Next block   : 0x"
        << realResult.nextBlock << '\n'
        << "  Instructions : "
        << std::dec
        << realResult.instructionsExecuted
        << '\n';

    std::cout << "\nReal PES loop test:\n";

    constexpr std::uint64_t loopStart =
        0x047d5024ULL;

    constexpr std::uint32_t loopGuestBase =
        0x10000000u;

    constexpr std::uint32_t loopListAddress =
        loopGuestBase + 0x100u;

    constexpr std::uint32_t loopTargetAddress =
        loopGuestBase + 0x300u;

    constexpr std::uint32_t loopStackAddress =
        loopGuestBase + 0x800u;

    constexpr std::uint32_t loopEaxValue = 5u;
    constexpr std::uint32_t loopAddValue = 7u;
    constexpr std::uint32_t oldEbpValue =
        0x55667788u;

    pes17::execution::MachineState loopState(
        loopGuestBase,
        0x1000);

    loopState.reg(pes17::ir::Register::EAX) =
        loopEaxValue;

    loopState.reg(pes17::ir::Register::ECX) =
        loopListAddress;

    loopState.reg(pes17::ir::Register::EBP) =
        loopStackAddress;

    bool loopMemoryReady = true;

    loopMemoryReady &=
        loopState.memory.write32(
            loopListAddress,
            loopTargetAddress);

    loopMemoryReady &=
        loopState.memory.write32(
            loopListAddress + 4,
            loopAddValue);

    loopMemoryReady &=
        loopState.memory.write32(
            loopListAddress + 8,
            0);

    loopMemoryReady &=
        loopState.memory.write32(
            loopTargetAddress,
            0);

    loopMemoryReady &=
        loopState.memory.write32(
            loopStackAddress,
            oldEbpValue);

    if (!loopMemoryReady) {
        std::cerr
            << "Real PES loop test FAILED: "
            << "memory setup\n";
        return 35;
    }

    pes17::execution::FunctionExecutor
        loopExecutor(loopState);

    const auto loopResult =
        loopExecutor.execute(
            lifted,
            loopStart,
            16);

    if (!loopResult.success) {
        std::cerr
            << "Real PES loop test FAILED: "
            << loopResult.error
            << '\n';
        return 36;
    }

    if (!loopResult.returned) {
        std::cerr
            << "Real PES loop test FAILED: "
            << "function did not return\n";
        return 37;
    }

    const auto loopStoredValue =
        loopState.memory.read32(
            loopTargetAddress);

    if (!loopStoredValue) {
        std::cerr
            << "Real PES loop test FAILED: "
            << "target read\n";
        return 38;
    }

    const std::uint32_t expectedStoredValue =
        loopEaxValue + loopAddValue;

    if (*loopStoredValue != expectedStoredValue) {
        std::cerr
            << "Real PES loop test FAILED: "
            << "target value 0x"
            << std::hex
            << *loopStoredValue
            << '\n';
        return 39;
    }

    if (loopState.reg(
            pes17::ir::Register::ECX) !=
        loopListAddress + 8) {

        std::cerr
            << "Real PES loop test FAILED: "
            << "ECX advancement\n";
        return 40;
    }

    if (loopState.reg(
            pes17::ir::Register::EBP) !=
        oldEbpValue) {

        std::cerr
            << "Real PES loop test FAILED: "
            << "EBP restoration\n";
        return 41;
    }

    if (loopState.reg(
            pes17::ir::Register::ESP) !=
        loopStackAddress + 4) {

        std::cerr
            << "Real PES loop test FAILED: "
            << "ESP restoration\n";
        return 42;
    }

    if (loopResult.blocksExecuted != 4) {
        std::cerr
            << "Real PES loop test FAILED: "
            << "expected 4 blocks, got "
            << loopResult.blocksExecuted
            << '\n';
        return 43;
    }

    std::cout
        << "Real PES loop test: PASS\n"
        << "  Start block  : 0x"
        << std::hex
        << loopStart
        << '\n'
        << "  Blocks       : "
        << std::dec
        << loopResult.blocksExecuted
        << '\n'
        << "  Instructions : "
        << loopResult.instructionsExecuted
        << '\n'
        << "  Target value : 0x"
        << std::hex
        << *loopStoredValue
        << '\n'
        << "  ECX final    : 0x"
        << loopState.reg(
            pes17::ir::Register::ECX)
        << '\n'
        << "  EBP restored : 0x"
        << loopState.reg(
            pes17::ir::Register::EBP)
        << '\n'
        << "  ESP final    : 0x"
        << loopState.reg(
            pes17::ir::Register::ESP)
        << '\n'
        << std::dec;

    std::cout << "\nC++ codegen test:\n";

    pes17::codegen::CppGenerator cppGenerator;

    const auto generatedCode =
        cppGenerator.generate(
            lifted,
            "pes17_fn_047d5000",
            0x047d5024ULL);

    if (!generatedCode.success) {
        std::cerr
            << "C++ codegen FAILED: "
            << generatedCode.error
            << '\n';
        return 50;
    }

    const char* generatedPath =
        "/storage/emulated/0/Download/"
        "PES17_NATIVE_RECOMP/generated/functions/"
        "pes17_fn_047d5000.cpp";

    std::ofstream generatedFile(
        generatedPath,
        std::ios::binary);

    if (!generatedFile) {
        std::cerr
            << "C++ codegen FAILED: output file\n";
        return 51;
    }

    generatedFile << generatedCode.source;
    generatedFile.close();

    std::cout
        << "C++ codegen: PASS\n"
        << "  Function : pes17_fn_047d5000\n"
        << "  Output   : "
        << generatedPath
        << '\n'
        << "  Bytes    : "
        << generatedCode.source.size()
        << '\n';

    std::cout << "\nIR verification:\n";

    pes17::ir::Verifier verifier;
    const auto verification =
        verifier.verify(lifted);

    std::cout
        << "Valid  : "
        << (verification.valid ? "YES" : "NO")
        << '\n'
        << "Errors : "
        << verification.errors
        << '\n';

    for (const auto& message :
         verification.messages) {
        std::cout
            << "  - "
            << message
            << '\n';
    }

    if (!verification.valid)
        return 12;

    return 0;
}
