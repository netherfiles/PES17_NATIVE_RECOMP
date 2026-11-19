#include "function_executor_test.h"

#include <cstdint>
#include <iostream>

#include "function_executor.h"

namespace pes17::execution {

namespace {

ir::Operand regOperand(ir::Register reg) {
    ir::Operand op;
    op.kind = ir::OperandKind::Register;
    op.reg = reg;
    op.width = 32;
    return op;
}

ir::Operand immOperand(std::uint32_t value) {
    ir::Operand op;
    op.kind = ir::OperandKind::Immediate;
    op.immediate = value;
    op.width = 32;
    return op;
}

ir::Function buildTestFunction() {
    constexpr std::uint64_t entry = 0x20000000;
    constexpr std::uint64_t body  = 0x20000010;
    constexpr std::uint64_t exit  = 0x20000020;

    ir::Function function;
    function.entry = entry;

    ir::Block entryBlock;
    entryBlock.start = entry;
    entryBlock.successors = {body, exit};

    ir::Instruction cmp;
    cmp.opcode = ir::Opcode::Compare;
    cmp.operands = {
        regOperand(ir::Register::EAX),
        immOperand(0)
    };

    ir::Instruction je;
    je.opcode = ir::Opcode::BranchIfEqual;
    je.hasTarget = true;
    je.target = exit;

    entryBlock.instructions = {cmp, je};

    ir::Block bodyBlock;
    bodyBlock.start = body;
    bodyBlock.successors = {exit};

    ir::Instruction add;
    add.opcode = ir::Opcode::Add;
    add.operands = {
        regOperand(ir::Register::EBX),
        immOperand(5)
    };

    ir::Instruction jump;
    jump.opcode = ir::Opcode::Branch;
    jump.hasTarget = true;
    jump.target = exit;

    bodyBlock.instructions = {add, jump};

    ir::Block exitBlock;
    exitBlock.start = exit;

    ir::Instruction ret;
    ret.opcode = ir::Opcode::Return;

    exitBlock.instructions = {ret};

    function.blocks = {
        entryBlock,
        bodyBlock,
        exitBlock
    };

    return function;
}

bool runCase(
    std::uint32_t eax,
    std::uint32_t initialEbx,
    std::uint32_t expectedEbx,
    std::size_t expectedBlocks) {

    MachineState state(0x10000000, 0x1000);

    state.reg(ir::Register::EAX) = eax;
    state.reg(ir::Register::EBX) = initialEbx;

    const auto function =
        buildTestFunction();

    FunctionExecutor executor(state);

    const auto result =
        executor.execute(
            function,
            function.entry,
            16);

    if (!result.success ||
        !result.returned) {
        return false;
    }

    if (result.blocksExecuted != expectedBlocks)
        return false;

    return state.reg(ir::Register::EBX) ==
           expectedEbx;
}

}

bool runFunctionExecutorSelfTest() {
    const bool taken =
        runCase(
            0,
            10,
            10,
            2);

    const bool fallthrough =
        runCase(
            1,
            10,
            15,
            3);

    if (!taken) {
        std::cout
            << "FunctionExecutor self-test FAILED: "
            << "taken path\n";
        return false;
    }

    if (!fallthrough) {
        std::cout
            << "FunctionExecutor self-test FAILED: "
            << "fallthrough path\n";
        return false;
    }

    std::cout
        << "FunctionExecutor self-test: PASS\n"
        << "  EAX=0 -> JE -> RET -> EBX=10\n"
        << "  EAX=1 -> ADD 5 -> RET -> EBX=15\n";

    return true;
}

}
