#include "isa/isa_ega.h"
#include "isa/isa_bus.h"
#include <cstring>
#include <fstream>
#include <spdlog/spdlog.h>

namespace bench {

ISA_EGA::ISA_EGA() : Component("EGA") {
    load_rom("assets/karta ega bios phenix.BIN");
}

void ISA_EGA::load_rom(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (f) {
        f.read(reinterpret_cast<char*>(rom_), ROM_SIZE);
        rom_loaded_ = true;
        spdlog::info("[EGA] loaded video BIOS: {}", path);
    } else {
        spdlog::warn("[EGA] video BIOS not found: {} -- no INT 10h support", path);
    }
}

void ISA_EGA::on_power_on() {
    std::memset(vram_, 0, sizeof(vram_));
    std::memset(latch_, 0, sizeof(latch_));
    misc_ = 0;
    feature_ = 0;
    crt_int_ = false;
    seq_index_ = 0;
    std::memset(seq_, 0, sizeof(seq_));
    crtc_index_ = 0;
    std::memset(crtc_, 0, sizeof(crtc_));
    gc_index_ = 0;
    std::memset(gc_, 0, sizeof(gc_));
    gpos1_ = 0;
    gpos2_ = 0;
    attr_index_ = 0;
    attr_flip_ = false;
    palette_source_ = false;
    std::memset(attr_, 0, sizeof(attr_));
    dot_acc_ = 0;
    dot_counter_ = 0;
    hcc_ = 0;
    line_ = 0;
    ra_ = 0;
    row_ma_ = 0;
    scanline_ = 0;
    in_vsync_ = false;
    vsync_left_ = 0;
    active_start_ = 0;
    active_start_set_ = false;
    std::memset(scanline_regs_, 0, sizeof(scanline_regs_));
    QueryPerformanceFrequency(&qpc_freq_);
    QueryPerformanceCounter(&qpc_start_);
}

// =========================================================================
// Claims
// =========================================================================

bool ISA_EGA::claims_port(uint16_t port) {
    // 3B0-3BB (mono CRTC/status base; 3BC-3BF is the printer port on MDA
    // cards, not EGA), 3C0-3CF (EGA proper), 3D0-3DF (color base).
    return (port >= 0x3B0 && port <= 0x3BB) ||
           (port >= 0x3C0 && port <= 0x3CF) ||
           (port >= 0x3D0 && port <= 0x3DF);
}

bool ISA_EGA::claims_mmio(uint32_t addr) {
    // Whole A0000-BFFFF region (window position decided at access time
    // by Graphics Misc bits 2-3) plus the video BIOS at C0000.
    return (addr >= 0xA0000 && addr < 0xC0000) ||
           (addr >= ROM_BASE && addr < ROM_BASE + ROM_SIZE);
}

// =========================================================================
// Memory decode
// =========================================================================

bool ISA_EGA::map_offset(uint32_t addr, uint32_t& off) const {
    if (!(misc_ & 0x02))       // Misc Output bit 1: Enable RAM
        return false;
    switch ((gc_[GFX_MISC] >> 2) & 3) {
        case 0:  // A0000, 128K
            if (addr >= 0xA0000 && addr < 0xC0000) { off = (addr - 0xA0000) & 0xFFFF; return true; }
            break;
        case 1:  // A0000, 64K
            if (addr >= 0xA0000 && addr < 0xB0000) { off = addr - 0xA0000; return true; }
            break;
        case 2:  // B0000, 32K
            if (addr >= 0xB0000 && addr < 0xB8000) { off = addr - 0xB0000; return true; }
            break;
        case 3:  // B8000, 32K
            if (addr >= 0xB8000 && addr < 0xC0000) { off = addr - 0xB8000; return true; }
            break;
    }
    return false;
}

// =========================================================================
// I/O reads
// =========================================================================

uint8_t ISA_EGA::on_io_read(uint16_t port) {
    bool color = (misc_ & 0x01) != 0;  // CRTC/status base: 3Dx vs 3Bx

    switch (port) {
        // Input Status 0: switch sense + CRT interrupt status.
        // CLKSEL (Misc bits 2-3) selects which of the 4 DIP switches is
        // read on bit 4.
        case 0x3C2: {
            uint8_t st = 0;
            uint8_t clksel = (misc_ >> 2) & 3;
            if (SWITCHES & (8 >> clksel))
                st |= 0x10;
            // Bit 7 per the tech ref: "a logical 1 indicates video is
            // being displayed; a logical 0 indicates that vertical
            // retrace is occurring" -- live status, not a latched
            // interrupt flag (the IRQ2 line latch is separate).
            if (!in_vsync_)
                st |= 0x80;
            return st;
        }

        // Input Status 1: display enable + vertical retrace. Reading it
        // resets the attribute controller flip-flop.
        case 0x3BA:
        case 0x3DA: {
            if (color != (port == 0x3DA))
                return 0xFF;
            attr_flip_ = false;
            uint8_t st = 0;
            // Bit 0 is the EGA's one status inversion vs CGA/VGA: the
            // tech ref defines it as the REAL display enable ("logical
            // 0 indicates the raster is in a retrace interval"), and
            // the EGA BIOS listing's snow-avoidance loop (write-char,
            // TEST AL,1 / JNZ twice) waits for bit 0 = 0 before writing
            // VRAM. So 1 = active display, 0 = blanking/retrace.
            if (displaying())
                st |= 0x01;
            if (in_vsync_)
                st |= 0x08;    // 1 = vertical retrace (same as CGA/VGA)
            return st;
        }

        // CRTC data: only the address-holding pairs are readable
        // (Start Address, Cursor Location; Light Pen reads as 0 --
        // no light pen attached).
        case 0x3B5:
        case 0x3D5: {
            if (color != (port == 0x3D5))
                return 0xFF;
            switch (crtc_index_) {
                case CRTC_START_H: case CRTC_START_L:
                case CRTC_CURSOR_H: case CRTC_CURSOR_L:
                    return crtc_[crtc_index_];
                case 0x10: case 0x11:  // light pen high/low
                    return 0;
                default:
                    return 0xFF;
            }
        }

        default:
            return 0xFF;  // everything else is write-only on the EGA
    }
}

// =========================================================================
// I/O writes
// =========================================================================

void ISA_EGA::on_io_write(uint16_t port, uint8_t val) {
    bool color = (misc_ & 0x01) != 0;

    switch (port) {
        case 0x3C2:
            misc_ = val;
            spdlog::debug("[EGA] misc=0x{:02X} (io={}, clk={}, page={})",
                         val, (val & 1) ? "3Dx" : "3Bx", (val >> 2) & 3, (val >> 5) & 1);
            return;

        // Feature Control (write at the status-1 address).
        case 0x3BA:
        case 0x3DA:
            if (color == (port == 0x3DA))
                feature_ = val;
            return;

        // Sequencer.
        case 0x3C4:
            seq_index_ = val & 0x07;
            return;
        case 0x3C5:
            if (seq_index_ < 5) {
                if (seq_[seq_index_] != val)
                    spdlog::debug("[EGA] seq[{}]=0x{:02X}", seq_index_, val);
                seq_[seq_index_] = val;
            }
            return;

        // Graphics Controller.
        case 0x3CE:
            gc_index_ = val & 0x0F;
            return;
        case 0x3CF:
            if (gc_index_ < 9) {
                // Mode-shaping registers only (5=mode, 6=misc); the write
                // pipeline registers churn constantly during drawing.
                if ((gc_index_ == GFX_MODE || gc_index_ == GFX_MISC) &&
                    gc_[gc_index_] != val)
                    spdlog::debug("[EGA] gc[{}]=0x{:02X}", gc_index_, val);
                gc_[gc_index_] = val;
            }
            return;
        case 0x3CC:
            gpos1_ = val;
            return;
        case 0x3CA:
            gpos2_ = val;
            return;

        // Attribute Controller: single port, index/data flip-flop.
        case 0x3C0:
            if (!attr_flip_) {
                attr_index_ = val & 0x1F;
                palette_source_ = (val & 0x20) != 0;
            } else {
                if (attr_index_ < 20) {
                    if (attr_index_ >= 0x10 && attr_[attr_index_] != val)
                        spdlog::debug("[EGA] attr[0x{:02X}]=0x{:02X}", attr_index_, val);
                    attr_[attr_index_] = val;
                }
            }
            attr_flip_ = !attr_flip_;
            return;

        // CRTC.
        case 0x3B4:
        case 0x3D4:
            if (color == (port == 0x3D4))
                crtc_index_ = val & 0x1F;
            return;
        case 0x3B5:
        case 0x3D5:
            if (color != (port == 0x3D5))
                return;
            if (crtc_index_ < 25) {
                // Timing registers only (start addr / cursor location
                // churn on every scroll and cursor move).
                if ((crtc_index_ < CRTC_START_H || crtc_index_ > CRTC_CURSOR_L) &&
                    crtc_[crtc_index_] != val)
                    spdlog::debug("[EGA] crtc[{}]=0x{:02X}", crtc_index_, val);
                crtc_[crtc_index_] = val;
                // Vertical Retrace End bit 4: a 0 clears the vertical
                // interrupt latch (and the IRQ2 line).
                if (crtc_index_ == CRTC_VSYNC_E && !(val & 0x10)) {
                    crt_int_ = false;
                    if (bus())
                        bus()->lower_irq(2);
                }
            }
            return;

        default:
            return;
    }
}

// =========================================================================
// MMIO: planar memory pipeline
// =========================================================================

uint8_t ISA_EGA::on_mmio_read(uint32_t addr) {
    if (addr >= ROM_BASE && addr < ROM_BASE + ROM_SIZE)
        return rom_[addr - ROM_BASE];

    uint32_t off;
    if (!map_offset(addr, off))
        return 0xFF;

    // Odd/even read chaining (Graphics Mode bit 4): address bit 0
    // selects the odd or even plane of the pair chosen by Read Map
    // Select bit 1.
    bool oe = (gc_[GFX_MODE] & 0x10) != 0;
    uint32_t eff = oe ? (off & ~1u) : off;

    // Latches load from all 4 planes on every read.
    latch_[0] = vram_[0][eff];
    latch_[1] = vram_[1][eff];
    latch_[2] = vram_[2][eff];
    latch_[3] = vram_[3][eff];

    if (gc_[GFX_MODE] & 0x08) {
        // Read mode 1: color compare. Result bit n = 1 where the 4
        // plane bits at position n match Color Compare on every plane
        // not excluded by Color Don't Care.
        uint8_t cmp = gc_[GFX_COLOR_CMP] & 0x0F;
        uint8_t care = gc_[GFX_COLOR_DC] & 0x0F;
        uint8_t result = 0xFF;
        for (int p = 0; p < 4; ++p) {
            if (!(care & (1 << p)))
                continue;
            uint8_t want = (cmp & (1 << p)) ? 0xFF : 0x00;
            result &= ~(latch_[p] ^ want);
        }
        return result;
    }

    // Read mode 0: plane from Read Map Select.
    uint32_t plane = gc_[GFX_READ_MAP] & 0x03;
    if (oe)
        plane = (plane & 0x02) | (off & 1);
    return latch_[plane];
}

void ISA_EGA::on_mmio_write(uint32_t addr, uint8_t val) {
    if (addr >= ROM_BASE)  // ROM (claims stop at C3FFF)
        return;

    uint32_t off;
    if (!map_offset(addr, off))
        return;

    uint8_t planes = seq_[SEQ_MAP_MASK] & 0x0F;
    // Odd/even write chaining (Sequencer Memory Mode bit 2 = 0): even
    // addresses go to planes 0/2, odd to planes 1/3.
    bool oe = !(seq_[SEQ_MEM_MODE] & 0x04);
    uint32_t eff = off;
    if (oe) {
        planes &= (off & 1) ? 0x0A : 0x05;
        eff = off & ~1u;
    }

    uint8_t wm = gc_[GFX_MODE] & 0x03;
    uint8_t func = (gc_[GFX_ROTATE] >> 3) & 0x03;
    uint8_t mask = gc_[GFX_BIT_MASK];

    auto alu = [func](uint8_t v, uint8_t l) -> uint8_t {
        switch (func) {
            case 1:  return v & l;
            case 2:  return v | l;
            case 3:  return v ^ l;
            default: return v;
        }
    };

    switch (wm) {
        case 0: {
            uint8_t rot = ror8(val, gc_[GFX_ROTATE] & 0x07);
            for (int p = 0; p < 4; ++p) {
                if (!(planes & (1 << p)))
                    continue;
                uint8_t v = (gc_[GFX_ENABLE_SR] & (1 << p))
                    ? ((gc_[GFX_SET_RESET] & (1 << p)) ? 0xFF : 0x00)
                    : rot;
                v = alu(v, latch_[p]);
                vram_[p][eff] = (v & mask) | (latch_[p] & ~mask);
#if BENCH_CFG_TRACE
                trace_plane_write((uint8_t)p, eff, vram_[p][eff]);
#endif
            }
            break;
        }
        case 1:
            // Latches copied straight to memory (no rotate/ALU/mask).
            for (int p = 0; p < 4; ++p)
                if (planes & (1 << p)) {
                    vram_[p][eff] = latch_[p];
#if BENCH_CFG_TRACE
                    trace_plane_write((uint8_t)p, eff, vram_[p][eff]);
#endif
                }
            break;
        case 2:
            // Color expand: data bit n fills plane n.
            for (int p = 0; p < 4; ++p) {
                if (!(planes & (1 << p)))
                    continue;
                uint8_t v = (val & (1 << p)) ? 0xFF : 0x00;
                v = alu(v, latch_[p]);
                vram_[p][eff] = (v & mask) | (latch_[p] & ~mask);
#if BENCH_CFG_TRACE
                trace_plane_write((uint8_t)p, eff, vram_[p][eff]);
#endif
            }
            break;
        default:
            break;  // write mode 3 is not valid on the EGA
    }
}

// Approximation: the EGA inserts wait states to synchronize CPU access
// with its own memory cycles (2-of-5 or 4-of-5 CRT slots per the
// Sequencer Bandwidth bit). Exact per-phase timing is not modeled yet;
// a fixed 2 CLKs is in the right ballpark for an 8-bit ISA video card.
uint32_t ISA_EGA::mmio_wait_clks(uint32_t addr) {
    if (addr >= ROM_BASE)
        return 0;
    return 2;
}

uint8_t ISA_EGA::debug_peek(uint32_t addr) const {
    if (addr >= ROM_BASE && addr < ROM_BASE + ROM_SIZE)
        return rom_[addr - ROM_BASE];
    uint32_t off;
    if (!map_offset(addr, off))
        return 0xFF;
    bool oe = (gc_[GFX_MODE] & 0x10) != 0;
    uint32_t eff = oe ? (off & ~1u) : off;
    uint32_t plane = gc_[GFX_READ_MAP] & 0x03;
    if (oe)
        plane = (plane & 0x02) | (off & 1);
    return vram_[plane][eff];
}

// =========================================================================
// GPU constants
// =========================================================================

void ISA_EGA::fill_gpu_constants(GpuConstants& cb) const {
    cb.cursor_ma = ((uint32_t)crtc_[CRTC_CURSOR_H] << 8) | crtc_[CRTC_CURSOR_L];
    cb.cursor_start = crtc_[CRTC_CURSOR_S] & 0x1F;
    cb.cursor_end = crtc_[CRTC_CURSOR_E] & 0x1F;

    LARGE_INTEGER qpc_now;
    QueryPerformanceCounter(&qpc_now);
    double elapsed = double(qpc_now.QuadPart - qpc_start_.QuadPart) / qpc_freq_.QuadPart;
    uint32_t frames = static_cast<uint32_t>(elapsed * EGA_FRAME_HZ);
    uint8_t counter = frames & 0x1F;
    cb.cursor_blink = (counter & 0x08) ? 1 : 0;
    cb.attr_blink = (counter & 0x10) ? 0 : 1;
    cb._pad[0] = cb._pad[1] = cb._pad[2] = 0;
}

// =========================================================================
// Beam simulation -- called every CLK cycle via the ISA bus DAG slot.
// =========================================================================

bool ISA_EGA::displaying() const {
    uint32_t h_disp = (uint32_t)crtc_[CRTC_HDISP_END] + 1;
    return hcc_ < h_disp && line_ < v_displayed_lines() && !in_vsync_;
}

void ISA_EGA::on_cycle(Fiber) {
    // Dot rate from Misc Output clock select, halved by Sequencer
    // Clocking Mode bit 3 (320-wide modes).
    uint32_t dot_hz = ((misc_ >> 2) & 3) == 0 ? 14318181u : 16257000u;
    if (seq_[SEQ_CLOCKING] & 0x08)
        dot_hz >>= 1;

    // Fractional dot accumulation: dots per CLK = dot_hz / CPU_HZ.
    dot_acc_ += dot_hz;
    uint32_t dots = dot_acc_ / CPU_HZ;
    dot_acc_ -= dots * CPU_HZ;

    uint32_t char_w = (seq_[SEQ_CLOCKING] & 0x01) ? 8 : 9;
    dot_counter_ += dots;
    while (dot_counter_ >= char_w) {
        dot_counter_ -= char_w;

        // --- One character clock ---
        hcc_++;
        uint32_t h_total = h_total_chars();
        if (h_total < 4) h_total = 4;   // unprogrammed CRTC guard
        if (hcc_ < h_total)
            continue;
        hcc_ = 0;
        end_scanline();
    }
}

void ISA_EGA::end_scanline() {
    stamp_scanline();

    uint32_t vt   = (((uint32_t)crtc_[CRTC_OVERFLOW] & 1) << 8) | crtc_[CRTC_VTOTAL];
    uint32_t vrs  = ((((uint32_t)crtc_[CRTC_OVERFLOW] >> 2) & 1) << 8) | crtc_[CRTC_VSYNC_S];
    uint32_t lcmp = ((((uint32_t)crtc_[CRTC_OVERFLOW] >> 4) & 1) << 8) | crtc_[CRTC_LINE_CMP];
    uint32_t max_sl = crtc_[CRTC_MAX_SCAN] & 0x1F;
    if (vt == 0) vt = 262;              // unprogrammed CRTC guard

    line_++;
    if (++scanline_ >= FRAME_LINES)
        scanline_ = FRAME_LINES - 1;

    // Row scan counter: new character row when RA passes Max Scan Line.
    if (ra_ >= max_sl) {
        ra_ = 0;
        row_ma_ += 2u * crtc_[CRTC_OFFSET];  // Offset is a word address
    } else {
        ra_++;
    }

    // Line compare: the row starting at this scanline fetches from
    // address 0 (split screen immune to scrolling).
    if (line_ == lcmp) {
        row_ma_ = 0;
        ra_ = 0;
    }

    // Vertical retrace: starts when the line counter hits VRS; ends
    // when the low 4 bits match Vertical Retrace End.
    if (!in_vsync_ && line_ == vrs) {
        in_vsync_ = true;
        uint32_t width = ((crtc_[CRTC_VSYNC_E] & 0x0F) - (vrs & 0x0F)) & 0x0F;
        vsync_left_ = width ? width : 16;
        active_start_set_ = false;
        if (clk_cycles_)
            last_vsync_clk_ = *clk_cycles_;
        // Vertical interrupt: enabled by R11h bit 5 = 0, armed while
        // bit 4 = 1 (a 0 there holds the latch cleared).
        if (!(crtc_[CRTC_VSYNC_E] & 0x20) && (crtc_[CRTC_VSYNC_E] & 0x10)) {
            crt_int_ = true;
            if (bus())
                bus()->raise_irq(2);
        }
    } else if (in_vsync_ && --vsync_left_ == 0) {
        in_vsync_ = false;
        scanline_ = 0;   // buffer anchored at vertical retrace end
    }

    // Frame wrap.
    if (line_ >= vt) {
        line_ = 0;
        ra_ = crtc_[CRTC_PRESET_ROW] & 0x1F;
        row_ma_ = ((uint32_t)crtc_[CRTC_START_H] << 8) | crtc_[CRTC_START_L];
        if (!active_start_set_) {
            active_start_ = scanline_;
            active_start_set_ = true;
        }
    }
}

// CRTC memory address translation for the beam's row fetch.
uint32_t ISA_EGA::fetch_addr(uint32_t byte_ma) const {
    uint32_t a = byte_ma;
    // CRTC Mode Control bit 0 (CMS0): row scan bit 0 replaces MA13
    // (CGA-compatible 8K interleave).
    if (!(crtc_[CRTC_MODE] & 0x01))
        a = (a & ~(1u << 13)) | ((ra_ & 1u) << 13);
    // Bit 1: row scan bit 1 replaces MA14.
    if (!(crtc_[CRTC_MODE] & 0x02))
        a = (a & ~(1u << 14)) | (((ra_ >> 1) & 1u) << 14);
    return a & (PLANE_SIZE - 1);
}

void ISA_EGA::stamp_scanline() {
    ScanlineRegs& sr = scanline_regs_[scanline_];
    sr.attr_mode    = attr_[ATTR_MODE];
    sr.gc_misc      = gc_[GFX_MISC];
    sr.gc_mode      = gc_[GFX_MODE];
    sr.seq_clocking = seq_[SEQ_CLOCKING];
    sr.crtc_mode    = crtc_[CRTC_MODE];
    sr.ma           = row_ma_;
    sr.ra           = ra_;
    sr.h_displayed  = (uint32_t)crtc_[CRTC_HDISP_END] + 1;
    // Retrace delay skew (R5 bits 5-6) shifts the whole retrace pulse
    // later by 0-3 character clocks (IBM uses it for screen centering:
    // mode 10h programs 1, mode F programs 3).
    sr.hsync_pos    = hsync_pos_chars();
    sr.hsync_width  = ((crtc_[CRTC_HSYNC_E] & 0x1F) - (crtc_[CRTC_HSYNC_S] & 0x1F)) & 0x1F;
    sr.h_total      = h_total_chars();
    sr.pel_pan      = attr_[ATTR_PEL_PAN] & 0x0F;
    sr.plane_enable = attr_[ATTR_PLANE_EN] & 0x0F;
    sr.border       = attr_[ATTR_OVERSCAN] & 0x3F;
    // Character map select only works with the memory expansion
    // installed (Memory Mode bit 1); otherwise bank 0 is forced.
    sr.char_map     = (seq_[SEQ_MEM_MODE] & 0x02) ? seq_[SEQ_CHAR_MAP] : 0;
    sr.underline    = crtc_[CRTC_UNDERLINE] & 0x1F;
    for (int i = 0; i < 4; ++i) {
        sr.palette[i] = (uint32_t)attr_[i * 4]
                      | ((uint32_t)attr_[i * 4 + 1] << 8)
                      | ((uint32_t)attr_[i * 4 + 2] << 16)
                      | ((uint32_t)attr_[i * 4 + 3] << 24);
    }
    // Display enable: rows between VDE and VTOTAL are border. A frame
    // under 300 lines means 15.7 kHz CGA-compatible timing -- the
    // monitor decodes the color signal as RGBI (palette bit 4 =
    // intensity) instead of 6-bit rgbRGB. While the Palette Address
    // Source bit is 0 the CPU owns the palette RAM and video data
    // cannot address it -- active display blanks.
    sr.flags = (line_ < v_displayed_lines() ? 1u : 0u)
             | (frame_total_lines() < 300 ? 2u : 0u)
             | (palette_source_ ? 0u : 4u);
    sr._pad[0] = sr._pad[1] = sr._pad[2] = 0;

    // Capture the plane rows the beam fetches this scanline.
    // Word mode (CRTC Mode Control bit 6 = 0): byte address = MA * 2,
    // two bytes per character (text char/attr pairs). Byte mode:
    // byte address = MA, one byte per character.
    bool word_mode = !(crtc_[CRTC_MODE] & 0x40);
    uint32_t stride = word_mode ? 2 : 1;
    uint32_t base = word_mode ? (row_ma_ << 1) : row_ma_;
    uint32_t bytes = sr.h_displayed * stride;
    if (bytes > SCANLINE_ROW_BYTES) bytes = SCANLINE_ROW_BYTES;

    for (int p = 0; p < 4; ++p) {
        auto* dst = reinterpret_cast<uint8_t*>(sr.plane_row[p]);
        for (uint32_t i = 0; i < bytes; ++i)
            dst[i] = vram_[p][fetch_addr(base + i)];
        for (uint32_t i = bytes; i < SCANLINE_ROW_BYTES; ++i)
            dst[i] = 0;
    }
}

} // namespace bench
