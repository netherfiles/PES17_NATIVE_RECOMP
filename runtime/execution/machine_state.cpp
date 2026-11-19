#include "machine_state.h"

#include <cstring>
#include <stdexcept>

namespace pes17::execution {

GuestMemory::GuestMemory(
    std::uint32_t baseAddress,
    std::size_t size)
    : baseAddress_(baseAddress),
      data_(size, 0) {
}

std::optional<std::size_t>
GuestMemory::offsetFor(
    std::uint32_t address,
    std::size_t size) const {

    if (address < baseAddress_)
        return std::nullopt;

    const std::uint64_t offset =
        static_cast<std::uint64_t>(address) -
        baseAddress_;

    if (offset + size > data_.size())
        return std::nullopt;

    return static_cast<std::size_t>(offset);
}

bool GuestMemory::contains(
    std::uint32_t address,
    std::size_t size) const {

    return offsetFor(address, size).has_value();
}

std::optional<std::uint32_t>
GuestMemory::read32(
    std::uint32_t address) const {

    const auto offset =
        offsetFor(address, sizeof(std::uint32_t));

    if (!offset)
        return std::nullopt;

    std::uint32_t value = 0;

    std::memcpy(
        &value,
        data_.data() + *offset,
        sizeof(value));

    return value;
}

bool GuestMemory::write32(
    std::uint32_t address,
    std::uint32_t value) {

    const auto offset =
        offsetFor(address, sizeof(std::uint32_t));

    if (!offset)
        return false;

    std::memcpy(
        data_.data() + *offset,
        &value,
        sizeof(value));

    return true;
}

MachineState::MachineState(
    std::uint32_t memoryBase,
    std::size_t memorySize)
    : memory(memoryBase, memorySize) {
}

std::size_t MachineState::registerIndex(
    ir::Register reg) {

    switch (reg) {
        case ir::Register::EAX: return 0;
        case ir::Register::EBX: return 1;
        case ir::Register::ECX: return 2;
        case ir::Register::EDX: return 3;
        case ir::Register::ESI: return 4;
        case ir::Register::EDI: return 5;
        case ir::Register::EBP: return 6;
        case ir::Register::ESP: return 7;
        case ir::Register::None: break;
    }

    throw std::invalid_argument(
        "Invalid x86 register");
}

std::uint32_t& MachineState::reg(
    ir::Register reg) {

    return registers_[registerIndex(reg)];
}

std::uint32_t MachineState::reg(
    ir::Register reg) const {

    return registers_[registerIndex(reg)];
}

}
