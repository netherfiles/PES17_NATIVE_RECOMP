#include "verifier.h"

namespace pes17::ir {

namespace {

bool hasFlag(std::uint8_t mask, Flag flag) {
    return (mask & static_cast<std::uint8_t>(flag)) != 0;
}

}

VerificationResult Verifier::verify(
    const Function& function) const {

    VerificationResult result;

    for (const auto& block : function.blocks) {
        bool zfAvailable = false;

        for (const auto& instruction : block.instructions) {

            if (hasFlag(instruction.flagsWritten, Flag::ZF))
                zfAvailable = true;

            if ((instruction.opcode == Opcode::BranchIfEqual ||
                 instruction.opcode == Opcode::BranchIfNotEqual) &&
                hasFlag(instruction.flagsRead, Flag::ZF) &&
                !zfAvailable) {

                result.valid = false;
                ++result.errors;

                result.messages.push_back(
                    "Conditional branch reads ZF without "
                    "a producer in the same IR block");
            }
        }

        if (!block.instructions.empty()) {
            const auto& last = block.instructions.back();

            if ((last.opcode == Opcode::BranchIfEqual ||
                 last.opcode == Opcode::BranchIfNotEqual) &&
                block.successors.size() != 2) {

                result.valid = false;
                ++result.errors;

                result.messages.push_back(
                    "Conditional branch must have 2 successors");
            }

            if (last.opcode == Opcode::Branch &&
                block.successors.size() != 1) {

                result.valid = false;
                ++result.errors;

                result.messages.push_back(
                    "Direct branch must have 1 successor");
            }

            if (last.opcode == Opcode::Return &&
                !block.successors.empty()) {

                result.valid = false;
                ++result.errors;

                result.messages.push_back(
                    "Return block must have no successors");
            }
        }
    }

    return result;
}

}
