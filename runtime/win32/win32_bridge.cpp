#include "win32_bridge.h"

// CORREÇÃO AQUI: Apontando para a pasta execution
#include "../execution/machine_state.h"
#include "win32_abi.h"

namespace pes17::execution {

void BindWin32Memory(MachineState& state) {
    // host = hostBase + guest  =>  hostBase = (ponteiro do 1o byte) - (endereço guest do 1o byte).
    // Aritmética em uintptr_t: hostBase pode ser "negativo"; o wrap é intencional e correto.
    auto& mem = state.memory;
    const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(mem.data());
    const std::uintptr_t hostBase = first - static_cast<std::uintptr_t>(mem.base());
    Win32_ConfigureGuestMemory(reinterpret_cast<void*>(hostBase), mem.base(),
                               static_cast<std::uint64_t>(mem.base()) + mem.size());
}

bool DispatchWin32Thunk(MachineState& state, std::uint32_t target) {
    // View de 3 referências aos registradores REAIS: nada é copiado e nada precisa ser devolvido.
    GuestCPU cpu{ state.reg(ir::Register::EAX), state.reg(ir::Register::EDX), state.reg(ir::Register::ESP) };
    return Win32_DispatchThunk(cpu, target);
}

} // namespace pes17::execution
