#include "pe_image.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace pes17 {

namespace {

template <typename T>
bool readValue(
    const std::vector<std::uint8_t>& data,
    std::size_t offset,
    T& value) {

    if (offset + sizeof(T) > data.size())
        return false;

    std::memcpy(&value, data.data() + offset, sizeof(T));
    return true;
}

}

bool PeSection::executable() const {
    constexpr std::uint32_t IMAGE_SCN_MEM_EXECUTE = 0x20000000u;
    return (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
}

bool PeSection::containsRva(std::uint32_t rva) const {
    const std::uint64_t start = virtualAddress;
    const std::uint64_t span =
        std::max<std::uint32_t>(virtualSize, rawSize);
    const std::uint64_t end = start + span;

    return rva >= start && rva < end;
}

bool PeImage::load(const std::string& path) {
    valid_ = false;
    error_.clear();
    sections_.clear();
    data_.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error_ = "Could not open file";
        return false;
    }

    file.seekg(0, std::ios::end);
    const auto length = file.tellg();

    if (length <= 0) {
        error_ = "Empty file";
        return false;
    }

    file.seekg(0, std::ios::beg);
    data_.resize(static_cast<std::size_t>(length));

    if (!file.read(
            reinterpret_cast<char*>(data_.data()),
            static_cast<std::streamsize>(data_.size()))) {
        error_ = "Could not read file";
        data_.clear();
        return false;
    }

    if (data_.size() < 0x40 ||
        data_[0] != 'M' ||
        data_[1] != 'Z') {
        error_ = "Invalid DOS/MZ header";
        return false;
    }

    std::uint32_t peOffset = 0;
    if (!readValue(data_, 0x3c, peOffset)) {
        error_ = "Could not read e_lfanew";
        return false;
    }

    if (static_cast<std::size_t>(peOffset) + 24 > data_.size()) {
        error_ = "PE header outside file";
        return false;
    }

    if (data_[peOffset + 0] != 'P' ||
        data_[peOffset + 1] != 'E' ||
        data_[peOffset + 2] != 0 ||
        data_[peOffset + 3] != 0) {
        error_ = "Invalid PE signature";
        return false;
    }

    const std::size_t coff = peOffset + 4;
    std::uint16_t optionalSize = 0;

    if (!readValue(data_, coff + 0, machine_) ||
        !readValue(data_, coff + 2, sectionCount_) ||
        !readValue(data_, coff + 16, optionalSize)) {
        error_ = "Truncated COFF header";
        return false;
    }

    const std::size_t optional = coff + 20;

    if (optional + optionalSize > data_.size()) {
        error_ = "Truncated optional header";
        return false;
    }

    if (!readValue(data_, optional + 0, optionalMagic_) ||
        !readValue(data_, optional + 16, entryPointRva_)) {
        error_ = "Could not read optional header";
        return false;
    }

    if (optionalMagic_ != 0x10b) {
        error_ = "Executable is not PE32";
        return false;
    }

    if (!readValue(data_, optional + 28, imageBase_)) {
        error_ = "Could not read ImageBase";
        return false;
    }

    const std::size_t sectionTable = optional + optionalSize;
    constexpr std::size_t sectionHeaderSize = 40;

    if (sectionTable +
            static_cast<std::size_t>(sectionCount_) *
                sectionHeaderSize >
        data_.size()) {
        error_ = "Truncated section table";
        return false;
    }

    sections_.reserve(sectionCount_);

    for (std::uint16_t i = 0; i < sectionCount_; ++i) {
        const std::size_t off =
            sectionTable +
            static_cast<std::size_t>(i) *
                sectionHeaderSize;

        char nameRaw[9] = {};
        std::memcpy(nameRaw, data_.data() + off, 8);

        PeSection section;
        section.name = nameRaw;

        if (!readValue(data_, off + 8, section.virtualSize) ||
            !readValue(data_, off + 12, section.virtualAddress) ||
            !readValue(data_, off + 16, section.rawSize) ||
            !readValue(data_, off + 20, section.rawOffset) ||
            !readValue(data_, off + 36, section.characteristics)) {
            error_ = "Could not read section header";
            return false;
        }

        sections_.push_back(section);
    }

    valid_ = true;
    return true;
}

const PeSection*
PeImage::sectionForRva(std::uint32_t rva) const {
    for (const auto& section : sections_) {
        if (section.containsRva(rva))
            return &section;
    }

    return nullptr;
}

std::optional<std::uint32_t>
PeImage::rvaToFileOffset(std::uint32_t rva) const {
    const auto* section = sectionForRva(rva);

    if (!section)
        return std::nullopt;

    const std::uint64_t delta =
        static_cast<std::uint64_t>(rva) -
        section->virtualAddress;

    if (delta >= section->rawSize)
        return std::nullopt;

    const std::uint64_t result =
        static_cast<std::uint64_t>(section->rawOffset) +
        delta;

    if (result >= data_.size())
        return std::nullopt;

    return static_cast<std::uint32_t>(result);
}

std::vector<std::uint8_t>
PeImage::readBytesAtRva(
    std::uint32_t rva,
    std::size_t count) const {

    const auto offset = rvaToFileOffset(rva);

    if (!offset)
        return {};

    const std::size_t begin = *offset;

    if (begin >= data_.size())
        return {};

    const std::size_t available = data_.size() - begin;
    const std::size_t actual = std::min(count, available);

    return {
        data_.begin() + begin,
        data_.begin() + begin + actual
    };
}

}
