#include "basic_block_builder.h"

#include <algorithm>

namespace pes17::analysis {

BasicBlockBuilder::BasicBlockBuilder(
    const PeImage& image,
    const x86::Decoder& decoder)
    : image_(image),
      decoder_(decoder) {
}

std::optional<BasicBlock>
BasicBlockBuilder::build(
    std::uint64_t virtualAddress) const {

    if (virtualAddress < image_.imageBase())
        return std::nullopt;

    const std::uint64_t rva64 =
        virtualAddress - image_.imageBase();

    if (rva64 > 0xffffffffULL)
        return std::nullopt;

    const auto rva =
        static_cast<std::uint32_t>(rva64);

    const auto* section =
        image_.sectionForRva(rva);

    if (!section)
        return std::nullopt;

    if (!image_.rvaToFileOffset(rva))
        return std::nullopt;

    const std::uint64_t sectionEnd =
        static_cast<std::uint64_t>(
            image_.imageBase()) +
        section->virtualAddress +
        std::max(
            section->virtualSize,
            section->rawSize);

    const std::uint64_t available =
        sectionEnd > virtualAddress
            ? sectionEnd - virtualAddress
            : 0;

    if (available == 0)
        return std::nullopt;

    const std::size_t readSize =
        static_cast<std::size_t>(
            std::min<std::uint64_t>(
                available,
                4096));

    const auto code =
        image_.readBytesAtRva(
            rva,
            readSize);

    if (code.empty())
        return std::nullopt;

    const auto decoded =
        decoder_.decode(
            code.data(),
            code.size(),
            virtualAddress,
            256);

    if (decoded.empty())
        return std::nullopt;

    BasicBlock block;
    block.start = virtualAddress;

    for (const auto& insn : decoded) {
        block.instructions.push_back(insn);

        block.end =
            insn.address + insn.size;

        switch (insn.flow) {
            case x86::FlowType::Call:
                if (insn.hasTarget)
                    block.calls.push_back(
                        insn.target);
                break;

            case x86::FlowType::ConditionalJump:
                if (insn.hasTarget)
                    block.successors.push_back(
                        insn.target);

                block.successors.push_back(
                    insn.address + insn.size);

                return block;

            case x86::FlowType::Jump:
                if (insn.hasTarget)
                    block.successors.push_back(
                        insn.target);

                return block;

            case x86::FlowType::Return:
                return block;

            case x86::FlowType::Normal:
                break;
        }
    }

    return block;
}

}
