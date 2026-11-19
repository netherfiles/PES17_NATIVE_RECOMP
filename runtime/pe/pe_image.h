#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pes17 {

struct PeSection {
    std::string name;
    std::uint32_t virtualAddress = 0;
    std::uint32_t virtualSize = 0;
    std::uint32_t rawOffset = 0;
    std::uint32_t rawSize = 0;
    std::uint32_t characteristics = 0;

    bool executable() const;
    bool containsRva(std::uint32_t rva) const;
};

class PeImage {
public:
    bool load(const std::string& path);

    bool valid() const { return valid_; }
    std::uint16_t machine() const { return machine_; }
    std::uint16_t sectionCount() const { return sectionCount_; }
    std::uint32_t entryPointRva() const { return entryPointRva_; }
    std::uint32_t imageBase() const { return imageBase_; }
    std::uint16_t optionalMagic() const { return optionalMagic_; }

    const std::vector<PeSection>& sections() const { return sections_; }
    const std::string& error() const { return error_; }

    const PeSection* sectionForRva(std::uint32_t rva) const;

    std::optional<std::uint32_t>
    rvaToFileOffset(std::uint32_t rva) const;

    std::vector<std::uint8_t>
    readBytesAtRva(std::uint32_t rva, std::size_t count) const;

private:
    bool valid_ = false;
    std::uint16_t machine_ = 0;
    std::uint16_t sectionCount_ = 0;
    std::uint16_t optionalMagic_ = 0;
    std::uint32_t entryPointRva_ = 0;
    std::uint32_t imageBase_ = 0;

    std::vector<std::uint8_t> data_;
    std::vector<PeSection> sections_;
    std::string error_;
};

}
