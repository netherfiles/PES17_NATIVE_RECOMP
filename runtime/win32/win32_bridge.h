#pragma once
// Ponte MachineState <-> camada Win32. Header LEVE de propósito: o código gerado inclui só este
// arquivo (não win32_abi.h/win32_types.h, que definem macros como NULL/TRUE/FALSE/INFINITE).
#include <cstdint>

namespace pes17::execution {
class MachineState;

// Chamar UMA vez depois de criar o MachineState e antes de executar código recompilado.
// Aponta a camada Win32 para a memória real do guest. Requer GuestMemory::data()/base()/size().
void BindWin32Memory(MachineState& state);

// Chama um thunk Win32 sobre o estado real (sem copiar registradores).
// true  = 'target' era um import Win32; EAX/EDX/ESP já foram atualizados em 'state'.
// false = 'target' não é thunk (função recompilada do jogo); 'state' intacto.
bool DispatchWin32Thunk(MachineState& state, std::uint32_t target);
}
