#pragma once
#include "isa/isa_card.h"
#include <spdlog/spdlog.h>
#include <cstdint>
#include <cstring>

namespace bench {

// ISA RAM expansion card.
//
// Pure SRAM mapped into the 20-bit address space.  No I/O ports, no DMA,
// no IRQ -- just memory.  Used to expand a 256KB 5150 up to 640KB by
// mapping 384KB at 0x40000-0x9FFFF.
class ISA_RAM final : public ISA_Card {
public:
    // base: first byte address (must be 4KB-aligned for MMIO page map).
    // size: number of bytes (must be 4KB-aligned).
    ISA_RAM(uint32_t base, uint32_t size)
        : base_(base), size_(size) {
        ram_ = new uint8_t[size];
        std::memset(ram_, 0, size);
    }

    ~ISA_RAM() override { delete[] ram_; }

    const std::string& card_name() const override { return name_; }

    bool claims_port(uint16_t /*port*/) override { return false; }
    bool claims_mmio(uint32_t addr) override {
        return addr >= base_ && addr < base_ + size_;
    }

    uint8_t on_io_read(uint16_t /*port*/) override { return 0xFF; }
    void    on_io_write(uint16_t /*port*/, uint8_t /*val*/) override {}

    uint8_t on_mmio_read(uint32_t addr) override {
        return ram_[addr - base_];
    }
    void on_mmio_write(uint32_t addr, uint8_t val) override {
        spdlog::info("[RAM] write {:05X} = {:02X}", addr, val);
        ram_[addr - base_] = val;
    }

    // Direct access for debugger memory view.
    const uint8_t* data() const { return ram_; }
    uint32_t base() const { return base_; }
    uint32_t size() const { return size_; }

private:
    std::string name_{"RAM"};
    uint32_t base_;
    uint32_t size_;
    uint8_t* ram_;
};

} // namespace bench
