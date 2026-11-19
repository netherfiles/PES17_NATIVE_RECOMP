#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "../ir/ir.h"
#include "machine_state.h"

namespace pes17::execution {

struct ExecutionResult {
    bool success = true;
    bool returned = false;

    std::size_t instructionsExecuted = 0;

    std::uint64_t nextBlock = 0;

    std::string error;
};

class Executor {
public:
    explicit Executor(MachineState& state);

    ExecutionResult executeBlock(
        const ir::Block& block);

private:
    std::optional<std::uint32_t>
    readOperand(const ir::Operand& operand);

    bool writeOperand(
        const ir::Operand& operand,
        std::uint32_t value);

    std::optional<std::uint32_t>
    effectiveAddress(
        const ir::MemoryOperand& memory) const;

    MachineState& state_;
};

}
