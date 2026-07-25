#pragma once
#include "isa/isa_card.h"
#include "core/component.h"
#include <cereal/cereal.hpp>
#include <cstdint>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

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
class ISA_CGA final : public ISA_Card, public Component {
public:
    ISA_CGA();

    // Component overrides (clocked by ISA_Bus).
    void power_on() override { on_power_on(); }
    void power_off() override {}
    bool is_powered() const override { return true; }
    void on_cycle(Fiber) override;
protected:
    void subscribe_to(Signal&) override {}
public:

    const std::string& card_name() const override { return name_; }

    // --- Raw state accessors (read by GPU each frame) ---

    const uint8_t* vram() const { return vram_; }
    uint8_t mode_register() const { return mode_; }
    uint8_t color_register() const { return color_; }
    const uint8_t* crtc_regs() const { return crtc_reg_; }
    uint8_t crtc_index() const { return crtc_index_; }

    // Framebuffer base and size
    static constexpr uint32_t FB_BASE = 0xB8000;
    static constexpr uint32_t FB_SIZE = 0x4000;    // 16KB actual
    static constexpr uint32_t FB_WINDOW = 0x8000;  // 32KB mirrored aperture

    // --- GPU constant buffer layout (uploaded each frame) ---
    // Matches CGA_CB in cga_rasterize.hlsl
    // Must match HLSL cbuffer layout exactly (no arrays, no padding surprises).
    struct alignas(16) GpuConstants {
        uint32_t mode;           // mode control register (0x3D8)
        uint32_t color;          // color select register (0x3D9)
        uint32_t cursor_blink;  // 1 = cursor visible this frame (1/16 frame rate)
        uint32_t attr_blink;    // 1 = blink-attr chars visible (1/32 frame rate)
        uint32_t composite;     // 1 = composite decode, 0 = RGBI
        uint32_t start_addr;    // CRTC start address (R12:R13)
        uint32_t cursor_addr;   // CRTC cursor address (R14:R15)
        uint32_t cursor_start;  // cursor start scanline
        uint32_t cursor_end;    // cursor end scanline
        uint32_t cursor_enabled;
        uint32_t max_scanline;  // CRTC R9: character height = max_scanline + 1
        uint32_t h_displayed;   // CRTC R1: columns displayed
        uint32_t v_displayed;   // CRTC R6: rows displayed
        uint32_t h_total;       // CRTC R0: horizontal total (char clocks - 1)
        uint32_t hsync_pos;     // CRTC R2: horizontal sync position
        uint32_t hsync_width;   // CRTC R3 low nibble: horizontal sync width
        uint32_t v_total;       // CRTC R4: vertical total (char rows - 1)
        uint32_t vtotal_adj;    // CRTC R5: vertical total adjust (scanlines)
        uint32_t vsync_pos;     // CRTC R7: vertical sync position
        uint32_t _pad[2];       // align to 80 bytes (5x16)
    };

    // Fill a GpuConstants struct from current register state.
    void fill_gpu_constants(GpuConstants& cb) const;

    // --- Rasterized scanline buffer ---
    // The beam reads VRAM as it scans, like a real CRT hitting phosphor.
    // Each scanline captures: register state, 6845 counters, AND the
    // VRAM row the beam read at that moment.  The shader renders from
    // this accumulated buffer, not from live VRAM.
    static constexpr uint32_t FRAME_LINES = 262;
    // Max 128 chars * 2 bytes. R1 can legally exceed 80 (Area5150's wide
    // modes); the 6845 can't display more than R0+1 chars per line and
    // CGA's 80-col R0 is 113, so 128 covers everything reachable.
    static constexpr uint32_t SCANLINE_ROW_BYTES = 256;
    static constexpr uint32_t SCANLINE_ROW_U32S  = SCANLINE_ROW_BYTES / 4;  // 64

    struct ScanlineRegs {
        // Register state + counters (12 uint32s)
        uint32_t mode;
        uint32_t color;
        uint32_t ma;            // 6845 MA for this scanline
        uint32_t ra;            // 6845 RA (scanline within char row)
        uint32_t vcc;           // 6845 VCC (character row counter)
        uint32_t h_displayed;   // R1
        uint32_t v_displayed;   // R6
        uint32_t hsync_pos;     // R2
        uint32_t hsync_width;   // R3 low nibble
        uint32_t h_total;       // R0
        uint32_t _pad[2];
        // VRAM row captured by the beam (64 uint32s = 256 bytes)
        uint32_t vram_row[SCANLINE_ROW_U32S];
    };
    static_assert(sizeof(ScanlineRegs) == (12 + 64) * 4);  // 304 bytes

    const ScanlineRegs* scanline_regs() const { return scanline_regs_; }

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

    // --- 8x8 CGA character ROM (loaded from file, uploaded as SRV) ---
    static constexpr int FONT_SIZE = 2048;
    const uint8_t* font_rom() const { return font_rom_; }

private:
    uint8_t font_rom_[FONT_SIZE] = {};
    bool font_loaded_ = false;
    void load_font(const char* path);

public:
    // --- Composite mode ---
    bool composite_mode() const { return composite_; }
    void set_composite(bool on) { composite_ = on; }

    // Bind to the 8284A's CLK cycle counter for status register timing.
    void set_clk_counter(const uint64_t* clk) { clk_cycles_ = clk; }

    // Emulated CLK of the most recent vsync leading edge (for the
    // renderer's monitor vertical-oscillator model). Not serialized.
    uint64_t last_vsync_clk() const { return last_vsync_clk_; }

    void card_save(cereal::BinaryOutputArchive& ar) override { serialize(ar); }
    void card_load(cereal::BinaryInputArchive& ar) override { serialize(ar); }
    template <class Archive> void serialize(Archive& ar) {
        ar(cereal::binary_data(vram_, sizeof(vram_)),
           crtc_index_,
           cereal::binary_data(crtc_reg_, sizeof(crtc_reg_)),
           // lclk_phase_ deliberately not serialized: free-running wait-state
           // phase, re-established within 16 CLKs; keeps .b51 format stable.
           mode_, color_, composite_,
           dot_counter_, hcc_, scanline_, vcc_, ra_, ma_,
           vtadj_counter_, in_vtadj_, in_vsync_, vsync_counter_,
           active_start_, active_start_set_,
           cereal::binary_data(scanline_regs_, sizeof(scanline_regs_)));
    }

    // --- ISA_Card overrides ---
    void on_power_on() override;
    bool claims_port(uint16_t port) override;
    bool claims_mmio(uint32_t addr) override;
    uint8_t on_io_read(uint16_t port) override;
    void    on_io_write(uint16_t port, uint8_t val) override;
    uint8_t on_mmio_read(uint32_t addr) override;
    void    on_mmio_write(uint32_t addr, uint8_t val) override;
    uint32_t mmio_wait_clks(uint32_t addr) override;

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

    // CLK cycle counter (from 8284A, for status register timing)
    const uint64_t* clk_cycles_ = nullptr;
    uint64_t last_vsync_clk_ = 0;  // transient, not serialized

    // Blink timing: QPC wall clock, independent of frame rate.
    // CGA frame rate: 14.318 MHz / (912 * 262) = ~59.92 Hz.
    // Cursor blinks at 1/16 frame rate (~3.75 Hz).
    // Attribute blink at 1/32 frame rate (~1.875 Hz).
    LARGE_INTEGER qpc_freq_ = {};
    LARGE_INTEGER qpc_start_ = {};
    static constexpr double CGA_FRAME_HZ = 14318180.0 / (912.0 * 262.0);

    // Composite output mode
    bool composite_ = false;

    // Continuous 6845 beam state, ticked every 304 CLK (1 scanline).
    static constexpr uint32_t CLK_PER_LINE  = 304;       // 912 dots / 3
    static constexpr uint32_t CLK_PER_FRAME = 304 * 262;  // 79648 CLK/frame
    ScanlineRegs scanline_regs_[FRAME_LINES] = {};
    uint32_t dot_counter_ = 0;    // dot clock accumulator (3 per system CLK)
    uint8_t lclk_phase_ = 0;      // free-running 16-dot lclock phase (wait states)

    // --- CGA snow (80-col text CPU/CRTC memory contention) ---
    // In 80-column text mode the CRTC uses every VRAM slot, so a CPU
    // access steals the in-progress character fetch: the CPU's data
    // byte lands in the CRTC latch and is displayed for that one cell
    // on that one scanline. Events are collected as accesses happen and
    // applied to the captured row in stamp_scanline().
    // NOT serialized (transient, and .b51 format must stay stable).
    static constexpr int MAX_SNOW_EVENTS = 16;
    struct SnowEvent { uint8_t col; uint8_t attr_half; uint8_t byte; };
    SnowEvent snow_events_[MAX_SNOW_EVENTS] = {};
    int snow_event_count_ = 0;
    uint64_t last_snow_key_ = ~0ull;  // (addr<<1)|is_write of current bus cycle
    bool mmio_touched_ = false;       // an MMIO dispatch happened this CLK
    void note_snow(uint32_t addr, uint8_t byte, bool is_write);
    uint32_t hcc_ = 0;           // horizontal character counter (0..R0)
    uint32_t scanline_ = 0;      // current scanline 0..261
    uint32_t vcc_ = 0;           // vertical character counter
    uint32_t ra_ = 0;            // raster address (scanline within char row)
    uint32_t ma_ = 0;            // memory address (linear address for this row)
    uint32_t vtadj_counter_ = 0; // counts R5 adjust scanlines at frame end
    bool     in_vtadj_ = false;  // true while counting adjust scanlines
    bool     in_vsync_ = false;  // true during 16-scanline VSYNC pulse
    uint32_t vsync_counter_ = 0; // counts scanlines within VSYNC
    uint32_t active_start_ = 0;  // buffer scanline where VCC first hit 0 after VSYNC
    bool     active_start_set_ = false; // only record the first VCC=0 per monitor frame
public:
    uint32_t active_start_scanline() const { return active_start_; }

    // Live beam position (for debug overlay).
    uint32_t beam_scanline() const { return scanline_; }
    uint32_t beam_hcc() const { return hcc_; }
    uint32_t beam_dot() const { return dot_counter_; }
    uint32_t beam_vcc() const { return vcc_; }
    bool beam_in_vsync() const { return in_vsync_; }
private:
    void stamp_scanline();       // write current state into scanline_regs_[scanline_]
};

} // namespace bench
