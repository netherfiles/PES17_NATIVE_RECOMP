class GuestMemory {
public:
    GuestMemory(std::uint32_t baseAddress, std::size_t size);

    std::uint32_t baseAddress() const { return baseAddress_; }
    
    // ADICIONADO PARA A PONTE DO CLAUDE:
    std::uint32_t base() const { return baseAddress_; }
    std::uint8_t* data() { return data_.data(); }
    const std::uint8_t* data() const { return data_.data(); }

    std::size_t size() const { return data_.size(); }

    bool contains(std::uint32_t address, std::size_t size) const;
    std::optional<std::uint32_t> read32(std::uint32_t address) const;
    bool write32(std::uint32_t address, std::uint32_t value);

private:
    std::optional<std::size_t> offsetFor(std::uint32_t address, std::size_t size) const;
    std::uint32_t baseAddress_ = 0;
    std::vector<std::uint8_t> data_;
};
