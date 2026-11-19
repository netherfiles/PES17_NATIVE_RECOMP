#include "executor_test.h"

#include <cstdint>
#include <iostream>

#include "executor.h"

namespace pes17::execution {

namespace {

ir::Operand makeRegister(
    ir::Register reg) {

    ir::Operand operand;
    operand.kind = ir::OperandKind::Register;
    operand.reg = reg;
    operand.width = 32;
    return operand;
}

ir::Operand makeMemory(
    ir::Register base,
    std::int64_t displacement = 0) {

    ir::Operand operand;
    operand.kind = ir::OperandKind::Memory;
    operand.width = 32;
    operand.memory.base = base;
    operand.memory.displacement = displacement;
    operand.memory.width = 32;
    return operand;
}

}


bool runBranchSelfTest() {
    constexpr std::uint32_t base = 0x10000000;
    constexpr std::uint64_t takenTarget = 0x30000000;
    constexpr std::uint64_t fallthrough = 0x20000010;

    auto makeImmediate = [](std::uint32_t value) {
        ir::Operand op;
        op.kind = ir::OperandKind::Immediate;
        op.immediate = value;
        op.width = 32;
        return op;
    };

    auto makeReg = [](ir::Register reg) {
        ir::Operand op;
        op.kind = ir::OperandKind::Register;
        op.reg = reg;
        op.width = 32;
        return op;
    };

    auto runCase = [&](std::uint32_t eaxValue,
                       std::uint64_t expectedTarget,
                       bool expectedZF) {
        MachineState state(base, 0x1000);
        state.reg(ir::Register::EAX) = eaxValue;

        ir::Block block;
        block.start = 0x20000000;
        block.successors = {
            fallthrough,
            takenTarget
        };

        ir::Instruction cmp;
        cmp.opcode = ir::Opcode::Compare;
        cmp.operands = {
            makeReg(ir::Register::EAX),
            makeImmediate(0)
        };

        ir::Instruction je;
        je.opcode = ir::Opcode::BranchIfEqual;
        je.hasTarget = true;
        je.target = takenTarget;

        block.instructions = {cmp, je};

        Executor executor(state);
        const auto result =
            executor.executeBlock(block);

        return result.success &&
               result.nextBlock == expectedTarget &&
               state.flags.zf == expectedZF;
    };

    const bool taken =
        runCase(
            0,
            takenTarget,
            true);

    const bool notTaken =
        runCase(
            1,
            fallthrough,
            false);

    if (!taken) {
        std::cout
            << "Branch self-test FAILED: "
            << "JE taken case\n";
        return false;
    }

    if (!notTaken) {
        std::cout
            << "Branch self-test FAILED: "
            << "JE fallthrough case\n";
        return false;
    }

    std::cout
        << "Branch self-test: PASS\n"
        << "  CMP 0,0 -> ZF=1 -> JE taken\n"
        << "  CMP 1,0 -> ZF=0 -> fallthrough\n";

    return true;
}



bool runStackSelfTest() {
    constexpr std::uint32_t memoryBase = 0x10000000;
    constexpr std::uint32_t initialEsp = 0x10000800;
    constexpr std::uint32_t value = 0x12345678;

    MachineState state(memoryBase, 0x1000);

    state.reg(ir::Register::ESP) = initialEsp;
    state.reg(ir::Register::EAX) = value;
    state.reg(ir::Register::EBP) = 0;

    ir::Operand eax;
    eax.kind = ir::OperandKind::Register;
    eax.reg = ir::Register::EAX;
    eax.width = 32;

    ir::Operand ebp;
    ebp.kind = ir::OperandKind::Register;
    ebp.reg = ir::Register::EBP;
    ebp.width = 32;

    ir::Instruction push;
    push.opcode = ir::Opcode::Push;
    push.operands = {eax};

    ir::Instruction pop;
    pop.opcode = ir::Opcode::Pop;
    pop.operands = {ebp};

    ir::Instruction ret;
    ret.opcode = ir::Opcode::Return;

    ir::Block block;
    block.start = 0x21000000;
    block.instructions = {push, pop, ret};

    Executor executor(state);
    const auto result = executor.executeBlock(block);

    if (!result.success || !result.returned) {
        std::cout << "Stack self-test FAILED: execution\n";
        return false;
    }

    if (state.reg(ir::Register::EBP) != value) {
        std::cout << "Stack self-test FAILED: EBP\n";
        return false;
    }

    if (state.reg(ir::Register::ESP) != initialEsp) {
        std::cout << "Stack self-test FAILED: ESP\n";
        return false;
    }

    std::cout
        << "Stack self-test: PASS\n"
        << "  PUSH EAX -> POP EBP\n"
        << "  EBP = 0x" << std::hex
        << state.reg(ir::Register::EBP) << '\n'
        << "  ESP = 0x"
        << state.reg(ir::Register::ESP) << '\n'
        << std::dec;

    return true;
}


bool runExecutorSelfTest() {
    constexpr std::uint32_t memoryBase =
        0x10000000;

    constexpr std::size_t memorySize =
        0x1000;

    constexpr std::uint32_t testAddress =
        memoryBase + 0x100;

    constexpr std::uint32_t testValue =
        0x89abcdef;

    MachineState state(
        memoryBase,
        memorySize);

    state.reg(ir::Register::EAX) =
        testValue;

    state.reg(ir::Register::EBX) =
        testAddress;

    ir::Block block;
    block.start = 0x20000000;

    ir::Instruction store;
    store.opcode = ir::Opcode::Store;
    store.sourceAddress = block.start;
    store.operands = {
        makeMemory(ir::Register::EBX),
        makeRegister(ir::Register::EAX)
    };

    ir::Instruction load;
    load.opcode = ir::Opcode::Load;
    load.sourceAddress = block.start + 1;
    load.operands = {
        makeRegister(ir::Register::EDX),
        makeMemory(ir::Register::EBX)
    };

    ir::Instruction ret;
    ret.opcode = ir::Opcode::Return;
    ret.sourceAddress = block.start + 2;

    block.instructions = {
        store,
        load,
        ret
    };

    Executor executor(state);

    const auto result =
        executor.executeBlock(block);

    if (!result.success) {
        std::cout
            << "Executor self-test FAILED: "
            << result.error
            << '\n';
        return false;
    }

    if (!result.returned) {
        std::cout
            << "Executor self-test FAILED: "
            << "Return not detected\n";
        return false;
    }

    if (result.instructionsExecuted != 3) {
        std::cout
            << "Executor self-test FAILED: "
            << "wrong instruction count\n";
        return false;
    }

    if (state.reg(ir::Register::EDX) !=
        testValue) {

        std::cout
            << "Executor self-test FAILED: "
            << "EDX mismatch\n";
        return false;
    }

    const auto stored =
        state.memory.read32(testAddress);

    if (!stored || *stored != testValue) {
        std::cout
            << "Executor self-test FAILED: "
            << "memory mismatch\n";
        return false;
    }

    std::cout
        << "Executor self-test: PASS\n";

    std::cout
        << "  stored = 0x"
        << std::hex
        << *stored
        << '\n';

    std::cout
        << "  EDX    = 0x"
        << state.reg(ir::Register::EDX)
        << '\n';

    std::cout << std::dec;
    return true;
}

}
