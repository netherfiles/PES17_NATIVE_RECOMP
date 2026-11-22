#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "../ir/operand.h"

namespace pes17::execution {

struct Flags {
    bool zf = false;
    bool cf = false;
    bool sf = false;
    bool of = false;
};

class GuestMemory {
public:
    GuestMemory(
        std::uint32_t baseAddress,
        std::size_t size);

    std::uint32_t baseAddress() const {
        return baseAddress_;
    }

    // --- INÍCIO DA CORREÇÃO (métodos que o win32_bridge.cpp precisa) ---
    std::uint32_t base() const {
        return baseAddress_;
    }

    std::uint8_t* data() {
        return data_.data();
    }

    const std::uint8_t* data() const {
        return data_.data();
    }
    // --- FIM DA CORREÇÃO ---

    std::size_t size() const {
        return data_.size();
    }

    bool contains(
        std::uint32_t address,
        std::size_t size) const;

    std::optional<std::uint32_t>
    read32(std::uint32_t address) const;

    bool write32(
        std::uint32_t address,
        std::uint32_t value);

private:
    std::optional<std::size_t>
    offsetFor(
        std::uint32_t address,
        std::size_t size) const;

    std::uint32_t baseAddress_ = 0;
    std::vector<std::uint8_t> data_;
};

class MachineState {
public:
    MachineState(
        std::uint32_t memoryBase,
        std::size_t memorySize);

    std::uint32_t& reg(ir::Register reg);
    std::uint32_t reg(ir::Register reg) const;

    Flags flags;
    GuestMemory memory;

private:
    static std::size_t registerIndex(
        ir::Register reg);

    std::array<std::uint32_t, 9> registers_{};
};

} // namespace pes17::execution
