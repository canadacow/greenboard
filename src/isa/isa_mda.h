#pragma once
#include "isa/isa_card.h"
#include "debug/traced_writer.h"
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
class ISA_MDA final : public ISA_Card
#if BENCH_CFG_TRACE
                    , public TracedWriter
#endif
{
public:
    ISA_MDA();

    const std::string& card_name() const override { return name_; }

    uint8_t* framebuffer() { return fb_; }
    const uint8_t* crtc_regs() const { return crtc_reg_; }
    uint8_t mode_register() const { return mode_; }
    static constexpr uint32_t FB_BASE = 0xB0000;
    static constexpr uint32_t FB_SIZE = 4096;

    // CRTC register indices (MC6845).
    static constexpr int CRTC_CURSOR_START  = 10;
    static constexpr int CRTC_CURSOR_END    = 11;
    static constexpr int CRTC_START_ADDR_H  = 12;
    static constexpr int CRTC_START_ADDR_L  = 13;
    static constexpr int CRTC_CURSOR_H      = 14;
    static constexpr int CRTC_CURSOR_L      = 15;
    static constexpr int CRTC_MAX_SCANLINE  = 9;

    // ISA_Card overrides
    void on_power_on() override;
    bool claims_port(uint16_t port) override;
    bool claims_mmio(uint32_t addr) override;
    uint8_t on_io_read(uint16_t port) override;
    void    on_io_write(uint16_t port, uint8_t val) override;
    uint8_t on_mmio_read(uint32_t addr) override;
    void    on_mmio_write(uint32_t addr, uint8_t val) override;

private:
    std::string name_{"MDA"};
    uint8_t fb_[FB_SIZE] = {};

    // 6845 CRTC registers
    uint8_t crtc_index_ = 0;
    uint8_t crtc_reg_[18] = {};

    // Mode control register (port 0x3B8)
    uint8_t mode_ = 0;

    // Status register state
    uint32_t status_counter_ = 0;
};

} // namespace bench
