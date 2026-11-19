#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pes17::x86 {

enum class FlowType {
    Normal,
    Call,
    Jump,
    ConditionalJump,
    Return
};

struct DecodedInstruction {
    std::uint64_t address = 0;
    std::uint16_t size = 0;
    std::uint32_t instructionId = 0;

    std::vector<std::uint8_t> bytes;

    std::string mnemonic;
    std::string operands;

    struct Operand {
        enum class Kind {
            Invalid,
            Register,
            Immediate,
            Memory
        };

        Kind kind = Kind::Invalid;

        std::uint32_t reg = 0;
        std::int64_t immediate = 0;

        std::uint32_t memoryBase = 0;
        std::uint32_t memoryIndex = 0;
        std::int32_t memoryScale = 1;
        std::int64_t memoryDisplacement = 0;

        std::uint8_t size = 0;
    };

    std::vector<Operand> decodedOperands;

    FlowType flow = FlowType::Normal;

    bool hasTarget = false;
    std::uint64_t target = 0;
};

class Decoder {
public:
    Decoder();
    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    bool valid() const;

    std::vector<DecodedInstruction> decode(
        const std::uint8_t* code,
        std::size_t size,
        std::uint64_t virtualAddress,
        std::size_t maxInstructions = 64) const;

private:
    std::size_t handle_ = 0;
    bool valid_ = false;
};

const char* flowName(FlowType type);

}
