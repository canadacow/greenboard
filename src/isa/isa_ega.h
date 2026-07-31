#pragma once
#include "isa/isa_card.h"
#include "core/component.h"
#if BENCH_CFG_TRACE
#include "debug/traced_writer.h"
#endif
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

// ISA EGA (Enhanced Graphics Adapter) card.
//
// 256KB video RAM as 4 bit planes of 64KB (Graphics Memory Expansion +
// Graphics Memory Module Kit fully populated). 16KB video BIOS at C0000
// (Phoenix Enhanced Video BIOS clone from assets/).
//
// Register file (IBM EGA tech ref, August 2 1984):
//   3C2  W: Miscellaneous Output    R: Input Status 0 (switches, CRT int)
//   3xA  W: Feature Control         R: Input Status 1 (DE, vretrace;
//                                      resets attribute flip-flop)
//   3C4/3C5: Sequencer (5 regs)
//   3B4/3B5 or 3D4/3D5: CRTC (25 regs; base per Misc Output bit 0)
//   3CE/3CF: Graphics Controller (9 regs), 3CC/3CA: position regs
//   3C0:     Attribute Controller (index/data flip-flop, 20 regs)
//
// The memory window is decoded from Graphics Misc bits 2-3:
//   00: A0000 128K   01: A0000 64K   10: B0000 32K   11: B8000 32K
//
// The CPU-side memory pipeline implements the full EGA data flow:
// 4 latches loaded on every read, write modes 0-2, read modes 0-1,
// set/reset, data rotate, ALU function select, bit mask, map mask,
// odd/even chaining.
//
// Rendering is NOT done here. Like the CGA card, a beam simulation runs
// every CLK cycle and stamps per-scanline register state + the VRAM row
// the beam fetched (all 4 planes) into a ring buffer; a GPU compute
// shader (ega_display.cpp) renders from that buffer.
class ISA_EGA final : public ISA_Card, public Component
#if BENCH_CFG_TRACE
                    , public TracedWriter
#endif
{
public:
    ISA_EGA();

    // Component overrides (clocked by ISA_Bus).
    void power_on() override { on_power_on(); }
    void power_off() override {}
    bool is_powered() const override { return true; }
    void on_cycle(Fiber) override;
protected:
    void subscribe_to(Signal&) override {}
public:

    const std::string& card_name() const override { return name_; }

    // --- Geometry ---
    static constexpr uint32_t PLANE_SIZE = 0x10000;   // 64KB per plane
    static constexpr uint32_t VRAM_SIZE  = 4 * PLANE_SIZE;
    static constexpr uint32_t ROM_BASE   = 0xC0000;
    static constexpr uint32_t ROM_SIZE   = 0x4000;    // 16KB video BIOS

    // --- Raw state accessors (read by GPU each frame) ---
    // Planes concatenated: plane p at offset p * PLANE_SIZE.
    const uint8_t* vram() const { return &vram_[0][0]; }
    const uint8_t* rom() const { return rom_; }

    // --- Sequencer register indices ---
    static constexpr int SEQ_RESET        = 0;
    static constexpr int SEQ_CLOCKING     = 1;   // bit0: 0=9-dot 1=8-dot, bit3: dot clock /2
    static constexpr int SEQ_MAP_MASK     = 2;
    static constexpr int SEQ_CHAR_MAP     = 3;
    static constexpr int SEQ_MEM_MODE     = 4;   // bit2: 0=odd/even 1=sequential

    // --- CRTC register indices ---
    static constexpr int CRTC_HTOTAL      = 0;   // total chars - 2
    static constexpr int CRTC_HDISP_END   = 1;   // visible chars - 1
    static constexpr int CRTC_HBLANK_S    = 2;
    static constexpr int CRTC_HBLANK_E    = 3;
    static constexpr int CRTC_HSYNC_S     = 4;
    static constexpr int CRTC_HSYNC_E     = 5;   // bits 0-4
    static constexpr int CRTC_VTOTAL      = 6;   // scanlines/frame (bit 8 in overflow)
    static constexpr int CRTC_OVERFLOW    = 7;   // b0 VT8 b1 VDE8 b2 VRS8 b3 SVB8 b4 LC8
    static constexpr int CRTC_PRESET_ROW  = 8;
    static constexpr int CRTC_MAX_SCAN    = 9;
    static constexpr int CRTC_CURSOR_S    = 10;
    static constexpr int CRTC_CURSOR_E    = 11;
    static constexpr int CRTC_START_H     = 12;
    static constexpr int CRTC_START_L     = 13;
    static constexpr int CRTC_CURSOR_H    = 14;
    static constexpr int CRTC_CURSOR_L    = 15;
    static constexpr int CRTC_VSYNC_S     = 16;  // bit 8 in overflow
    static constexpr int CRTC_VSYNC_E     = 17;  // b0-3 end, b4 clear int, b5 enable int
    static constexpr int CRTC_VDISP_END   = 18;  // last visible scanline (bit 8 in overflow)
    static constexpr int CRTC_OFFSET      = 19;  // row pitch in words
    static constexpr int CRTC_UNDERLINE   = 20;
    static constexpr int CRTC_VBLANK_S    = 21;
    static constexpr int CRTC_VBLANK_E    = 22;
    static constexpr int CRTC_MODE        = 23;  // b0 CMS0, b1 row scan sel, b6 word/byte
    static constexpr int CRTC_LINE_CMP    = 24;

    // --- Graphics Controller register indices ---
    static constexpr int GFX_SET_RESET     = 0;
    static constexpr int GFX_ENABLE_SR     = 1;
    static constexpr int GFX_COLOR_CMP     = 2;
    static constexpr int GFX_ROTATE        = 3;   // b0-2 count, b3-4 function
    static constexpr int GFX_READ_MAP      = 4;
    static constexpr int GFX_MODE          = 5;   // b0-1 write mode, b3 read mode, b4 odd/even, b5 shift reg
    static constexpr int GFX_MISC          = 6;   // b0 graphics, b1 chain o/e, b2-3 memory map
    static constexpr int GFX_COLOR_DC      = 7;
    static constexpr int GFX_BIT_MASK      = 8;

    // --- Attribute Controller register indices ---
    static constexpr int ATTR_MODE        = 0x10; // b0 gfx, b1 mono, b2 line gfx, b3 blink
    static constexpr int ATTR_OVERSCAN    = 0x11;
    static constexpr int ATTR_PLANE_EN    = 0x12;
    static constexpr int ATTR_PEL_PAN     = 0x13;

    // --- GPU constant buffer (uploaded each frame) ---
    // Must match EGA_CB in ega_display.cpp exactly.
    struct alignas(16) GpuConstants {
        uint32_t cursor_ma;      // CRTC R14:R15 (MA units)
        uint32_t cursor_start;   // R10 & 0x1F
        uint32_t cursor_end;     // R11 & 0x1F
        uint32_t cursor_blink;   // 1 = cursor visible this frame
        uint32_t attr_blink;     // 1 = blink-attr chars visible
        uint32_t _pad[3];        // 32 bytes total
    };
    void fill_gpu_constants(GpuConstants& cb) const;

    // --- Per-scanline capture (beam racing, like ISA_CGA) ---
    // The beam sim stamps register state + the 4 plane rows fetched by
    // the CRTC each scanline. Frame buffer covers up to 512 scanlines
    // (EGA vertical counter is 9-bit): 364 lines in 350-line modes,
    // 262 in CGA-compatible 200-line modes.
    static constexpr uint32_t FRAME_LINES = 512;
    // Max fetch per scanline per plane: 128 chars x 2 bytes (word mode).
    static constexpr uint32_t SCANLINE_ROW_BYTES = 256;
    static constexpr uint32_t SCANLINE_ROW_U32S  = SCANLINE_ROW_BYTES / 4;  // 64

    struct ScanlineRegs {
        // Register state + counters (24 uint32s)
        uint32_t attr_mode;      // attribute 10h
        uint32_t gc_misc;        // graphics 06h
        uint32_t gc_mode;        // graphics 05h (shift register bit)
        uint32_t seq_clocking;   // sequencer 01h (8/9 dot, dot clock /2)
        uint32_t crtc_mode;      // CRTC 17h (word/byte mode)
        uint32_t ma;             // row start address (MA units)
        uint32_t ra;             // row scan counter
        uint32_t h_displayed;    // visible chars (R1+1)
        uint32_t hsync_pos;      // R4 (chars)
        uint32_t hsync_width;    // chars
        uint32_t h_total;        // chars per line (R0+2)
        uint32_t pel_pan;        // attribute 13h
        uint32_t plane_enable;   // attribute 12h low nibble
        uint32_t border;         // attribute 11h (6-bit color)
        uint32_t char_map;       // sequencer 03h
        uint32_t underline;      // CRTC 14h
        uint32_t palette[4];     // 16 palette regs, one byte each
        // bit0: display enable (0 = border row between VDE and VTOTAL)
        // bit1: RGBI monitor decode (200-line modes: palette bit 4 is
        //       the intensity bit, secondary bits ignored -- the monitor
        //       runs in CGA-compatible 15.7 kHz mode)
        // bit2: palette RAM owned by the CPU (Palette Address Source =
        //       0): active display blanks
        uint32_t flags;
        uint32_t _pad[3];
        // The 4 plane rows the beam fetched (64 uint32s = 256B each)
        uint32_t plane_row[4][SCANLINE_ROW_U32S];
    };
    static_assert(sizeof(ScanlineRegs) == (24 + 4 * 64) * 4);  // 1120 bytes

    const ScanlineRegs* scanline_regs() const { return scanline_regs_; }

    // First buffer scanline of the active frame (vertical crop origin).
    uint32_t active_start_scanline() const { return active_start_; }

    // --- Geometry helpers for the rasterizer's crop rect ---
    uint32_t h_total_chars() const { return (uint32_t)crtc_[CRTC_HTOTAL] + 2; }
    // Retrace start plus the R5 bits 5-6 delay skew (0-3 chars).
    uint32_t hsync_pos_chars() const {
        return (uint32_t)crtc_[CRTC_HSYNC_S] + ((crtc_[CRTC_HSYNC_E] >> 5) & 3);
    }
    // Output dots per character clock (pixel-doubled when dot clock /2).
    uint32_t dots_per_char_out() const {
        uint32_t w = (seq_[SEQ_CLOCKING] & 0x01) ? 8 : 9;
        return (seq_[SEQ_CLOCKING] & 0x08) ? w * 2 : w;
    }
    uint32_t h_displayed_dots() const {
        return ((uint32_t)crtc_[CRTC_HDISP_END] + 1) * dots_per_char_out();
    }
    uint32_t v_displayed_lines() const {
        return ((((uint32_t)crtc_[CRTC_OVERFLOW] >> 1) & 1) << 8 |
                crtc_[CRTC_VDISP_END]) + 1;
    }
    uint32_t frame_total_lines() const {
        uint32_t vt = (((uint32_t)crtc_[CRTC_OVERFLOW] & 1) << 8) | crtc_[CRTC_VTOTAL];
        return vt ? vt : 262;
    }

    // Bind to the 8284A's CLK cycle counter for vsync timestamping.
    void set_clk_counter(const uint64_t* clk) { clk_cycles_ = clk; }

    // Emulated CLK of the most recent vsync leading edge (for the
    // renderer's monitor vertical-oscillator model). Not serialized.
    uint64_t last_vsync_clk() const { return last_vsync_clk_; }

    // Live beam position (debug overlay).
    uint32_t beam_scanline() const { return scanline_; }
    uint32_t beam_hcc() const { return hcc_; }
    bool beam_in_vsync() const { return in_vsync_; }

    // Side-effect-free CPU-view read for the memory viewer (no latch load).
    uint8_t debug_peek(uint32_t addr) const;

    void card_save(cereal::BinaryOutputArchive& ar) override { serialize(ar); }
    void card_load(cereal::BinaryInputArchive& ar) override { serialize(ar); }
    template <class Archive> void serialize(Archive& ar) {
        ar(cereal::binary_data(&vram_[0][0], sizeof(vram_)),
           cereal::binary_data(latch_, sizeof(latch_)),
           misc_, feature_, crt_int_,
           seq_index_, cereal::binary_data(seq_, sizeof(seq_)),
           crtc_index_, cereal::binary_data(crtc_, sizeof(crtc_)),
           gc_index_, cereal::binary_data(gc_, sizeof(gc_)),
           gpos1_, gpos2_,
           attr_index_, attr_flip_, palette_source_,
           cereal::binary_data(attr_, sizeof(attr_)),
           dot_acc_, dot_counter_, hcc_, line_, ra_, row_ma_,
           scanline_, in_vsync_, vsync_left_,
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
    std::string name_{"EGA"};

    // 4 x 64KB bit planes
    uint8_t vram_[4][PLANE_SIZE] = {};
    // Processor data latches (loaded on every CPU read)
    uint8_t latch_[4] = {};
    // 16KB video BIOS ROM
    uint8_t rom_[ROM_SIZE] = {};
    bool rom_loaded_ = false;
    void load_rom(const char* path);

    // --- Registers ---
    uint8_t misc_ = 0;        // 3C2 write
    uint8_t feature_ = 0;     // 3xA write
    uint8_t seq_index_ = 0;
    uint8_t seq_[5] = {};
    uint8_t crtc_index_ = 0;
    uint8_t crtc_[25] = {};
    uint8_t gc_index_ = 0;
    uint8_t gc_[9] = {};
    uint8_t gpos1_ = 0;       // 3CC
    uint8_t gpos2_ = 0;       // 3CA
    uint8_t attr_index_ = 0;
    bool attr_flip_ = false;  // false = next 3C0 write is index
    bool palette_source_ = false;  // 3C0 index bit 5
    uint8_t attr_[20] = {};

    // Configuration DIP switches (4 bits). 0x09 = primary EGA with
    // Enhanced Color Display (the standard emulator setting; read one
    // bit at a time via Misc Output CLKSEL + Input Status 0 bit 4).
    static constexpr uint8_t SWITCHES = 0x09;

    // Vertical interrupt pending (Input Status 0 bit 7, IRQ2).
    bool crt_int_ = false;

    // CLK cycle counter (from 8284A); vsync timestamp is transient.
    const uint64_t* clk_cycles_ = nullptr;
    uint64_t last_vsync_clk_ = 0;

    // --- Beam state ---
    // System CLK is 4.772727 MHz; the EGA dot clock is 14.318181 or
    // 16.257 MHz (Misc Output clock select), optionally divided by 2
    // (Sequencer Clocking Mode bit 3). Neither is an integer multiple
    // of CLK, so dots are accumulated in Hz units.
    static constexpr uint32_t CPU_HZ = 4772727;
    uint32_t dot_acc_ = 0;      // Hz accumulator
    uint32_t dot_counter_ = 0;  // dots within current character clock
    uint32_t hcc_ = 0;          // horizontal character counter
    uint32_t line_ = 0;         // scanline counter within frame
    uint32_t ra_ = 0;           // row scan counter
    uint32_t row_ma_ = 0;       // current row start address (MA units)
    uint32_t scanline_ = 0;     // buffer scanline index
    bool in_vsync_ = false;
    uint32_t vsync_left_ = 0;
    uint32_t active_start_ = 0;
    bool active_start_set_ = false;
    ScanlineRegs scanline_regs_[FRAME_LINES] = {};

    // Blink timing (QPC wall clock, like the CGA card).
    LARGE_INTEGER qpc_freq_ = {};
    LARGE_INTEGER qpc_start_ = {};
    static constexpr double EGA_FRAME_HZ = 60.0;

    // --- Helpers ---
    void end_scanline();
    void stamp_scanline();
    // CRTC MA-to-plane-byte-address translation for the row fetch:
    // word/byte mode plus the CMS0 / row-scan-counter address
    // substitutions (CRTC Mode Control bits 0-1) used by the
    // CGA-compatible interleaved modes.
    uint32_t fetch_addr(uint32_t byte_ma) const;
    // Decode a CPU address against the current memory map. Returns
    // false if outside the window (or RAM disabled).
    bool map_offset(uint32_t addr, uint32_t& off) const;
    bool displaying() const;
    static uint8_t ror8(uint8_t v, unsigned n) {
        n &= 7;
        return (uint8_t)((v >> n) | (v << (8 - n)));
    }
};

} // namespace bench
