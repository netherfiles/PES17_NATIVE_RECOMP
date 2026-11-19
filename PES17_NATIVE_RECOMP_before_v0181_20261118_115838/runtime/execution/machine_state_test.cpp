#include "machine_state_test.h"

#include <cstdint>
#include <iostream>

#include "machine_state.h"

namespace pes17::execution {

bool runMachineStateSelfTest() {
    constexpr std::uint32_t base = 0x10000000;
    constexpr std::size_t size = 0x1000;
    constexpr std::uint32_t address = base + 0x20;
    constexpr std::uint32_t expected = 0x12345678;

    MachineState state(base, size);

    state.reg(ir::Register::EAX) = 0x11223344;
    state.reg(ir::Register::ECX) = address;

    if (state.reg(ir::Register::EAX) != 0x11223344) {
        std::cout << "MachineState test: EAX FAILED\n";
        return false;
    }

    if (!state.memory.write32(address, expected)) {
        std::cout << "MachineState test: write32 FAILED\n";
        return false;
    }

    const auto value = state.memory.read32(address);

    if (!value || *value != expected) {
        std::cout << "MachineState test: read32 FAILED\n";
        return false;
    }

    if (state.memory.read32(base + size)) {
        std::cout << "MachineState test: bounds FAILED\n";
        return false;
    }

    state.flags.zf = true;

    if (!state.flags.zf) {
        std::cout << "MachineState test: ZF FAILED\n";
        return false;
    }

    std::cout << "MachineState self-test: PASS\n";
    std::cout << "  EAX    = 0x"
              << std::hex
              << state.reg(ir::Register::EAX)
              << '\n';

    std::cout << "  memory = 0x"
              << *value
              << '\n';

    std::cout << std::dec;
    return true;
}

}
