#pragma once

#include <cstdint>
#include <optional>

#include "basic_block.h"
#include "../pe/pe_image.h"
#include "../x86/decoder.h"

namespace pes17::analysis {

class BasicBlockBuilder {
public:
    BasicBlockBuilder(
        const PeImage& image,
        const x86::Decoder& decoder);

    std::optional<BasicBlock>
    build(std::uint64_t virtualAddress) const;

private:
    const PeImage& image_;
    const x86::Decoder& decoder_;
};

}
