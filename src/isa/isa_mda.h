#pragma once
#include "isa/isa_adapter.h"
#include <cstdint>

namespace bench {

// ISA MDA (Monochrome Display Adapter) card.
//
// 4KB framebuffer at 0xB0000-0xB0FFF.
// Character/attribute pairs: even bytes = character, odd bytes = attribute.
// 80x25 text mode = 4000 bytes used.
//
// I/O ports 0x3B0-0x3BB:
//   0x3B0/0x3B1: 6845 CRTC index/data registers
//   0x3B8: Mode control register
//   0x3BA: Status register (bit 0 = hsync, bit 3 = video)
//
// Logs characters written to the screen at info level.
class ISA_MDA final : public ISA_Adapter {
public:
    ISA_MDA();

    uint8_t* framebuffer() { return fb_; }
    static constexpr uint32_t FB_BASE = 0xB0000;
    static constexpr uint32_t FB_SIZE = 4096;

protected:
    void on_power_on() override;

    bool claims_port(uint16_t port) override;
    bool claims_mmio(uint32_t addr) override;
    uint8_t on_io_read(uint16_t port) override;
    void    on_io_write(uint16_t port, uint8_t val) override;
    uint8_t on_mmio_read(uint32_t addr) override;
    void    on_mmio_write(uint32_t addr, uint8_t val) override;
    uint8_t on_dma_read() override { return 0xFF; }
    void    on_dma_complete(int channel) override {}

private:
    uint8_t fb_[FB_SIZE] = {};

    // 6845 CRTC registers
    uint8_t crtc_index_ = 0;
    uint8_t crtc_reg_[18] = {};

    // Mode control register (port 0x3B8)
    uint8_t mode_ = 0;

    // Status register state -- hsync/vsync toggle on reads
    uint32_t status_counter_ = 0;
};

} // namespace bench
