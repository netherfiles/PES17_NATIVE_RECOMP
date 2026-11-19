#pragma once

#include "../analysis/function.h"
#include "ir.h"

namespace pes17::ir {

class X86Lifter {
public:
    Function lift(
        const analysis::Function& function) const;

private:
    Instruction liftInstruction(
        const x86::DecodedInstruction& instruction) const;
};

}
