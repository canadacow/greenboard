#pragma once
#include "isa/isa_card.h"
#include <cstdint>

namespace bench {

// ISA CGA (Color Graphics Adapter) card.
//
// 16KB framebuffer at 0xB8000-0xBFFFF (mirrored).
// Character/attribute pairs in text mode, pixel data in graphics mode.
//
// Modes:
//   0/1: 40x25 text (16 colors, 8x8 font, pixel-doubled)
//   2/3: 80x25 text (16 colors, 8x8 font)
//   4/5: 320x200 graphics (4 colors from 2 palettes)
//   6:   640x200 graphics (2 colors)
//
// I/O ports 0x3D0-0x3DF:
//   0x3D4/0x3D5: MC6845 CRTC index/data
//   0x3D8: Mode control register
//   0x3D9: Color select register
//   0x3DA: Status register (hsync, vsync)
//
// Rendering is NOT done by this class. The card exposes raw VRAM + registers.
// A GPU compute shader (cga_rasterize.hlsl) handles:
//   - Mode decode (text/graphics, all resolutions)
//   - 8x8 font glyph rendering
//   - Attribute decode (fg/bg/blink/intensity)
//   - Graphics palette + interleaved scanline addressing
//   - Composite NTSC artifact color (optional pass)
//   - Aspect-correct upscale to display resolution
class ISA_CGA final : public ISA_Card {
public:
    ISA_CGA();

    const std::string& card_name() const override { return name_; }

    // --- Raw state accessors (read by GPU each frame) ---

    const uint8_t* vram() const { return vram_; }
    uint8_t mode_register() const { return mode_; }
    uint8_t color_register() const { return color_; }
    const uint8_t* crtc_regs() const { return crtc_reg_; }
    uint8_t crtc_index() const { return crtc_index_; }
    bool blink_state() const { return blink_on_; }

    // Framebuffer base and size
    static constexpr uint32_t FB_BASE = 0xB8000;
    static constexpr uint32_t FB_SIZE = 0x4000;    // 16KB actual
    static constexpr uint32_t FB_WINDOW = 0x8000;  // 32KB mirrored aperture

    // --- GPU constant buffer layout (uploaded each frame) ---
    // Matches CGA_CB in cga_rasterize.hlsl
    struct alignas(16) GpuConstants {
        uint32_t mode;           // mode control register (0x3D8)
        uint32_t color;          // color select register (0x3D9)
        uint32_t crtc[18];      // MC6845 registers (widened to uint32 for HLSL)
        uint32_t blink_on;      // 1 = blink visible, 0 = blink hidden
        uint32_t composite;     // 1 = composite decode, 0 = RGBI
        uint32_t start_addr;    // CRTC start address (R12:R13)
        uint32_t cursor_addr;   // CRTC cursor address (R14:R15)
        uint32_t cursor_start;  // cursor start scanline
        uint32_t cursor_end;    // cursor end scanline
        uint32_t cursor_enabled;
        uint32_t _pad[2];       // align to 16-byte boundary
    };

    // Fill a GpuConstants struct from current register state.
    void fill_gpu_constants(GpuConstants& cb) const;

    // --- CRTC register indices ---
    static constexpr int CRTC_HTOTAL           = 0;
    static constexpr int CRTC_HDISPLAYED       = 1;
    static constexpr int CRTC_HSYNC_POS        = 2;
    static constexpr int CRTC_SYNC_WIDTH       = 3;
    static constexpr int CRTC_VTOTAL           = 4;
    static constexpr int CRTC_VTOTAL_ADJ       = 5;
    static constexpr int CRTC_VDISPLAYED       = 6;
    static constexpr int CRTC_VSYNC_POS        = 7;
    static constexpr int CRTC_INTERLACE        = 8;
    static constexpr int CRTC_MAX_SCANLINE     = 9;
    static constexpr int CRTC_CURSOR_START     = 10;
    static constexpr int CRTC_CURSOR_END       = 11;
    static constexpr int CRTC_START_ADDR_H     = 12;
    static constexpr int CRTC_START_ADDR_L     = 13;
    static constexpr int CRTC_CURSOR_H         = 14;
    static constexpr int CRTC_CURSOR_L         = 15;
    static constexpr int CRTC_LIGHT_PEN_H      = 16;
    static constexpr int CRTC_LIGHT_PEN_L      = 17;

    // --- Mode control register bits (0x3D8) ---
    static constexpr uint8_t MODE_HIRES_TEXT   = 0x01;
    static constexpr uint8_t MODE_GRAPHICS     = 0x02;
    static constexpr uint8_t MODE_BW           = 0x04;
    static constexpr uint8_t MODE_ENABLE       = 0x08;
    static constexpr uint8_t MODE_HIRES_GFX    = 0x10;
    static constexpr uint8_t MODE_BLINK        = 0x20;

    // --- Color select register bits (0x3D9) ---
    static constexpr uint8_t CC_COLOR_MASK     = 0x0F;
    static constexpr uint8_t CC_BRIGHT         = 0x10;
    static constexpr uint8_t CC_PALETTE        = 0x20;

    // --- Standard CGA 16-color RGBI palette ---
    struct Color { uint8_t r, g, b; };
    static constexpr Color PALETTE[16] = {
        {0x00,0x00,0x00}, {0x00,0x00,0xAA}, {0x00,0xAA,0x00}, {0x00,0xAA,0xAA},
        {0xAA,0x00,0x00}, {0xAA,0x00,0xAA}, {0xAA,0x55,0x00}, {0xAA,0xAA,0xAA},
        {0x55,0x55,0x55}, {0x55,0x55,0xFF}, {0x55,0xFF,0x55}, {0x55,0xFF,0xFF},
        {0xFF,0x55,0x55}, {0xFF,0x55,0xFF}, {0xFF,0xFF,0x55}, {0xFF,0xFF,0xFF},
    };

    // --- 4-color graphics palettes ---
    static constexpr uint8_t GFX_PALETTES[6][4] = {
        {0, 2, 4, 6},     {0, 10, 12, 14},
        {0, 3, 5, 7},     {0, 11, 13, 15},
        {0, 3, 4, 7},     {0, 11, 12, 15},
    };

    // --- 8x8 CGA character ROM (uploaded as SRV) ---
    static const uint8_t FONT_8X8[2048];

    // --- Composite mode ---
    bool composite_mode() const { return composite_; }
    void set_composite(bool on) { composite_ = on; }

    // --- ISA_Card overrides ---
    void on_power_on() override;
    bool claims_port(uint16_t port) override;
    bool claims_mmio(uint32_t addr) override;
    uint8_t on_io_read(uint16_t port) override;
    void    on_io_write(uint16_t port, uint8_t val) override;
    uint8_t on_mmio_read(uint32_t addr) override;
    void    on_mmio_write(uint32_t addr, uint8_t val) override;

private:
    std::string name_{"CGA"};

    // 16KB Video RAM
    uint8_t vram_[FB_SIZE] = {};

    // MC6845 CRTC registers
    uint8_t crtc_index_ = 0;
    uint8_t crtc_reg_[18] = {};

    // Control registers
    uint8_t mode_ = 0;
    uint8_t color_ = 0;

    // Status register state
    uint32_t status_counter_ = 0;

    // Blink state (toggled by status register reads)
    uint32_t blink_counter_ = 0;
    bool blink_on_ = true;

    // Composite output mode
    bool composite_ = false;
};

} // namespace bench
