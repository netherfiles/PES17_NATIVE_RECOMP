case Opcode::And:
case Opcode::Or:
case Opcode::ShiftRight:
case Opcode::AddFpu:
case Opcode::MulFpu:
case Opcode::DivFpu:
case Opcode::MoveAligned:
    // Por enquanto, vamos gerar um comentário no C++ para não quebrar o build
    code << "/* Simulando instrucao matematica/fpu avancada */\n";
    break;
    
case Opcode::PushFpu:
case Opcode::PopFpu:
    code << "/* Simulando push/pop da FPU */\n";
    break;
