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
