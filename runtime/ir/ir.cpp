#include "ir.h"

namespace pes17::ir {

const char* opcodeName(Opcode opcode) {
    switch (opcode) {
        case Opcode::Push: return "Push";
        case Opcode::Pop: return "Pop";
        case Opcode::Load: return "Load";
        case Opcode::Store: return "Store";
        case Opcode::Move: return "Move";
        case Opcode::Address: return "Address";
        case Opcode::Add: return "Add";
        case Opcode::Subtract: return "Subtract";
        case Opcode::Compare: return "Compare";
        case Opcode::CallDirect: return "CallDirect";
        case Opcode::CallIndirect: return "CallIndirect";
        case Opcode::Branch: return "Branch";
        case Opcode::BranchIfEqual: return "BranchIfEqual";
        case Opcode::BranchIfNotEqual: return "BranchIfNotEqual";
        case Opcode::Return: return "Return";
        
        // Nomes das novas instruções
        case Opcode::Increment: return "Increment";
        case Opcode::Xor: return "Xor";
        case Opcode::ShiftLeft: return "ShiftLeft";
        case Opcode::Test: return "Test";
        case Opcode::MoveZeroExtend: return "MoveZeroExtend";
        case Opcode::BranchIfAbove: return "BranchIfAbove";
        case Opcode::SetIfEqual: return "SetIfEqual";
        case Opcode::SubtractWithBorrow: return "SubtractWithBorrow";
        case Opcode::Exchange: return "Exchange";
        case Opcode::PushFlags: return "PushFlags";
        
        case Opcode::Unsupported: return "Unsupported";
    }
    return "Unknown";
}

} // namespace pes17::ir
