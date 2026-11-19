#include "ir.h"

namespace pes17::ir {

const char* opcodeName(Opcode opcode) {
    switch (opcode) {
        case Opcode::Unsupported:
            return "Unsupported";

        case Opcode::Push:
            return "Push";

        case Opcode::Pop:
            return "Pop";

        case Opcode::Move:
            return "Move";

        case Opcode::Address:
            return "Address";

        case Opcode::Load:
            return "Load";

        case Opcode::Store:
            return "Store";

        case Opcode::Add:
            return "Add";

        case Opcode::Subtract:
            return "Subtract";

        case Opcode::Compare:
            return "Compare";

        case Opcode::CallDirect:
            return "CallDirect";

        case Opcode::CallIndirect:
            return "CallIndirect";

        case Opcode::Branch:
            return "Branch";

        case Opcode::BranchIfEqual:
            return "BranchIfEqual";

        case Opcode::BranchIfNotEqual:
            return "BranchIfNotEqual";

        case Opcode::Return:
            return "Return";
    }

    return "Unknown";
}

}
