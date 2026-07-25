#include "isa/isa_cga.h"
#include <cstring>
#include <fstream>
#include <spdlog/spdlog.h>

namespace bench {

ISA_CGA::ISA_CGA() : Component("CGA") {
    load_font("assets/IBM_CGA-8.raw");
}

void ISA_CGA::load_font(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (f) {
        f.read(reinterpret_cast<char*>(font_rom_), FONT_SIZE);
        font_loaded_ = true;
        spdlog::info("[CGA] loaded font ROM: {}", path);
    } else {
        spdlog::warn("[CGA] font ROM not found: {} -- using blank font", path);
    }
}

void ISA_CGA::on_power_on() {
    std::memset(vram_, 0, FB_SIZE);
    std::memset(crtc_reg_, 0, sizeof(crtc_reg_));
    crtc_index_ = 0;
    mode_ = 0;
    color_ = 0;
    composite_ = false;
    dot_counter_ = 0;
    hcc_ = 0;
    scanline_ = 0;
    vcc_ = 0;
    ra_ = 0;
    ma_ = 0;
    vtadj_counter_ = 0;
    in_vtadj_ = false;
    in_vsync_ = false;
    vsync_counter_ = 0;
    active_start_ = 0;
    active_start_set_ = false;
    std::memset(scanline_regs_, 0, sizeof(scanline_regs_));
    QueryPerformanceFrequency(&qpc_freq_);
    QueryPerformanceCounter(&qpc_start_);
}

// =========================================================================
// Port claiming: 0x3D0-0x3DF
// =========================================================================

bool ISA_CGA::claims_port(uint16_t port) {
    return port >= 0x3D0 && port <= 0x3DF;
}

bool ISA_CGA::claims_mmio(uint32_t addr) {
    return addr >= FB_BASE && addr < FB_BASE + FB_WINDOW;
}

// =========================================================================
// I/O reads
// =========================================================================

uint8_t ISA_CGA::on_io_read(uint16_t port) {
    switch (port) {
        case 0x3D0: case 0x3D2: case 0x3D4: case 0x3D6:
            return crtc_index_;

        case 0x3D1: case 0x3D3: case 0x3D5: case 0x3D7:
            if (crtc_index_ < 18)
                return crtc_reg_[crtc_index_];
            return 0;

        case 0x3D8:
            return 0;

        case 0x3D9:
            return 0;

        // Status register -- derived from running beam state.
        case 0x3DA: {
            uint8_t status = 0;
            // Bit 0: ~DE. Display Enable is active when HCC < R1 AND VCC < R6.
            if (hcc_ >= crtc_reg_[CRTC_HDISPLAYED] ||
                vcc_ >= crtc_reg_[CRTC_VDISPLAYED])
                status |= 0x01;
            // Bit 3: VSYNC. Active during 16-scanline VSYNC pulse.
            if (in_vsync_)
                status |= 0x08;
            return status;
        }

        case 0x3DB: case 0x3DC:
            return 0;

        default:
            return 0xFF;
    }
}

// =========================================================================
// I/O writes -- just update registers, on_cycle() stamps scanlines.
// =========================================================================

// CPU accesses to CGA VRAM are synchronized to the card's lclock: the
// card holds I/O CH RDY low until its 16-dot cycle reaches the access
// grant slot, plus a fixed service latency. Constants match MartyPC's
// hardware-derived WAIT_TABLE: waits = ((5 - phase) & 15) + 9 dots,
// i.e. grant at lclock phase 5 with 9 dots of service time. This is
// what makes CPU code racing the beam cost the same cycles as real
// hardware (3-8 CPU cycles extra per VRAM access, phase-dependent).
uint32_t ISA_CGA::mmio_wait_clks(uint32_t /*addr*/) {
    uint32_t wait_dots = ((5u - lclk_phase_) & 15u) + 9u;
    return (wait_dots + 2) / 3;  // ceil to CPU CLKs
}

void ISA_CGA::on_io_write(uint16_t port, uint8_t val) {
    switch (port) {
        case 0x3D0: case 0x3D2: case 0x3D4: case 0x3D6:
            crtc_index_ = val & 0x1F;
            break;

        case 0x3D1: case 0x3D3: case 0x3D5: case 0x3D7:
            if (crtc_index_ < 18) {
                switch (crtc_index_) {
                    case CRTC_VTOTAL:      val &= 0x7F; break;
                    case CRTC_VDISPLAYED:  val &= 0x7F; break;
                    case CRTC_MAX_SCANLINE: val &= 0x1F; break;
                    case CRTC_CURSOR_START: val &= 0x3F; break;
                    case CRTC_CURSOR_END:   val &= 0x1F; break;
                    case CRTC_START_ADDR_H: val &= 0x3F; break;
                    default: break;
                }
                crtc_reg_[crtc_index_] = val;
            }
            break;

        case 0x3D8:
            mode_ = val;
            break;

        case 0x3D9:
            color_ = val;
            break;

        case 0x3DB: case 0x3DC:
            break;

        default:
            break;
    }
}

// =========================================================================
// MMIO: 16KB VRAM mirrored across 32KB window
// =========================================================================

uint8_t ISA_CGA::on_mmio_read(uint32_t addr) {
    uint8_t val = vram_[(addr - FB_BASE) & (FB_SIZE - 1)];
    note_snow(addr, val, false);
    return val;
}

void ISA_CGA::on_mmio_write(uint32_t addr, uint8_t val) {
    vram_[(addr - FB_BASE) & (FB_SIZE - 1)] = val;
    note_snow(addr, val, true);
}

// CGA snow: record a CPU VRAM access that steals the CRTC's fetch.
// Only 80-column text mode snows -- in 40-col and graphics modes the
// CGA interleaves CPU access into a free memory slot (that is what the
// wait states synchronize to), but 80-col text needs every slot.
// The stolen fetch shows the CPU's data byte in place of the character
// code or attribute, depending on which half of the character clock
// the access lands in.
void ISA_CGA::note_snow(uint32_t addr, uint8_t byte, bool is_write) {
    if ((mode_ & (MODE_HIRES_TEXT | MODE_GRAPHICS | MODE_ENABLE)) !=
        (MODE_HIRES_TEXT | MODE_ENABLE))
        return;

    mmio_touched_ = true;

    // The ISA bus dispatches level-based: the same bus cycle calls this
    // every CLK while ~MEMR/~MEMW is low. Collapse to one event.
    uint64_t key = ((uint64_t)addr << 1) | (is_write ? 1 : 0);
    if (key == last_snow_key_)
        return;
    last_snow_key_ = key;

    // Corruption is only visible while the beam is fetching characters.
    if (in_vsync_ || in_vtadj_)
        return;
    if (hcc_ >= crtc_reg_[CRTC_HDISPLAYED] || vcc_ >= crtc_reg_[CRTC_VDISPLAYED])
        return;
    if (snow_event_count_ >= MAX_SNOW_EVENTS)
        return;

    snow_events_[snow_event_count_++] = {
        (uint8_t)hcc_,
        (uint8_t)((dot_counter_ >> 2) & 1),  // char or attr half of the fetch
        byte
    };
}

// =========================================================================
// GPU constant buffer fill
// =========================================================================

void ISA_CGA::fill_gpu_constants(GpuConstants& cb) const {
    cb.mode = mode_;
    cb.color = color_;

    LARGE_INTEGER qpc_now;
    QueryPerformanceCounter(&qpc_now);
    double elapsed = double(qpc_now.QuadPart - qpc_start_.QuadPart) / qpc_freq_.QuadPart;
    uint32_t frames = static_cast<uint32_t>(elapsed * CGA_FRAME_HZ);
    uint8_t counter = frames & 0x1F;
    cb.cursor_blink = (counter & 0x08) ? 1 : 0;
    cb.attr_blink = (counter & 0x10) ? 0 : 1;

    cb.composite = composite_ ? 1 : 0;
    cb.start_addr = (crtc_reg_[CRTC_START_ADDR_H] << 8) | crtc_reg_[CRTC_START_ADDR_L];
    cb.cursor_addr = (crtc_reg_[CRTC_CURSOR_H] << 8) | crtc_reg_[CRTC_CURSOR_L];
    cb.cursor_start = crtc_reg_[CRTC_CURSOR_START] & 0x1F;
    cb.cursor_end = crtc_reg_[CRTC_CURSOR_END] & 0x1F;
    cb.cursor_enabled = ((crtc_reg_[CRTC_CURSOR_START] & 0x60) != 0x20) ? 1 : 0;
    cb.max_scanline = crtc_reg_[CRTC_MAX_SCANLINE] & 0x1F;
    cb.h_displayed = crtc_reg_[CRTC_HDISPLAYED];
    cb.v_displayed = crtc_reg_[CRTC_VDISPLAYED];
    cb.h_total = crtc_reg_[CRTC_HTOTAL];
    cb.hsync_pos = crtc_reg_[CRTC_HSYNC_POS];
    cb.hsync_width = crtc_reg_[CRTC_SYNC_WIDTH] & 0x0F;
    cb.v_total = crtc_reg_[CRTC_VTOTAL] & 0x7F;
    cb.vtotal_adj = crtc_reg_[CRTC_VTOTAL_ADJ] & 0x1F;
    cb.vsync_pos = crtc_reg_[CRTC_VSYNC_POS] & 0x7F;
    cb._pad[0] = cb._pad[1] = 0;
}

// =========================================================================
// Continuous 6845 beam simulation -- called every CLK cycle via DAG.
// =========================================================================

void ISA_CGA::on_cycle(Fiber) {
    // The 6845 CLK input is the character clock.  We receive system CLK
    // (4.77 MHz = 14.318 / 3).  Accumulate dots (3 per system CLK),
    // tick HCC when a full character clock has elapsed.
    //
    // 80-col / hi-res: 8 dots per char clock.
    // 40-col / lo-res: 16 dots per char clock.
    // CGA character clock: 8 dots if +HRES (80-col text OR 640x200 gfx),
    // 16 dots otherwise (40-col text, 320x200 gfx).
    bool hires = (mode_ & MODE_HIRES_TEXT) || (mode_ & MODE_HIRES_GFX);
    uint32_t dots_per_char = hires ? 8 : 16;

    // Free-running lclock phase (16 dots @ 14.318 MHz = 1.79 MHz), used
    // by the CPU-access wait-state synchronizer. Advances 3 dots per
    // system CLK regardless of CRTC state -- must tick before any
    // early return below.
    lclk_phase_ = (lclk_phase_ + 3) & 15;

    // Snow bookkeeping: a CLK with no MMIO dispatch means the current
    // CPU bus cycle ended (~MEMR/~MEMW went high between cycles), so a
    // later access to the same address is a new bus cycle and may snow
    // again.
    if (!mmio_touched_)
        last_snow_key_ = ~0ull;
    mmio_touched_ = false;

    dot_counter_ += 3;
    if (dot_counter_ < dots_per_char)
        return;
    dot_counter_ -= dots_per_char;

    // --- One character clock tick: HCC increments ---
    hcc_++;
    uint8_t htotal = crtc_reg_[CRTC_HTOTAL];

    if (hcc_ <= htotal)
        return;

    // --- HCC reached R0: scanline complete ---
    hcc_ = 0;

    // Stamp this scanline.
    stamp_scanline();

    // Tick vertical counters per the 6845 datasheet.
    //
    // RA increments each scanline.  When RA == R9 (coincidence), RA
    // resets to 0 and either VCC increments or the frame ends.
    //
    // Frame end logic (per datasheet): at the start of each
    // character row (RA==0), if VCC == R4, an internal "last row" flag
    // is set.  When RA reaches R9 at the end of that row, if "last row"
    // is set, VCC resets to 0, MA reloads from start_addr, and R5
    // adjust scanlines are counted.  Otherwise VCC increments normally.
    uint8_t max_sl = crtc_reg_[CRTC_MAX_SCANLINE] & 0x1F;
    uint8_t h_disp = crtc_reg_[CRTC_HDISPLAYED];
    uint8_t vtotal = crtc_reg_[CRTC_VTOTAL] & 0x7F;
    if (h_disp == 0) h_disp = 80;

    // --- 6845 vertical counter logic (per datasheet) ---

    if (in_vtadj_) {
        // R5 adjust phase: count individual scanlines.
        vtadj_counter_++;
        uint8_t vtotal_adj = crtc_reg_[CRTC_VTOTAL_ADJ] & 0x1F;
        if (vtadj_counter_ >= vtotal_adj) {
            in_vtadj_ = false;
            vtadj_counter_ = 0;
            vcc_ = 0;
            ra_ = 0;
            ma_ = (crtc_reg_[CRTC_START_ADDR_H] << 8) |
                   crtc_reg_[CRTC_START_ADDR_L];
        }
    } else if (ra_ >= max_sl) {
        ra_ = 0;
        if (vcc_ == vtotal) {
            uint8_t vtotal_adj = crtc_reg_[CRTC_VTOTAL_ADJ] & 0x1F;
            if (vtotal_adj == 0) {
                vcc_ = 0;
                ra_ = 0;
                ma_ = (crtc_reg_[CRTC_START_ADDR_H] << 8) |
                       crtc_reg_[CRTC_START_ADDR_L];
            } else {
                in_vtadj_ = true;
                vtadj_counter_ = 0;
            }
        } else {
            vcc_ = (vcc_ + 1) & 0x7F;  // 7-bit counter per 6845
            ma_ += h_disp;
        }
    } else {
        ra_++;
    }

    // --- VSYNC (per datasheet: VCC == R7, lasts 16 scanlines) ---
    uint8_t vsync_pos = crtc_reg_[CRTC_VSYNC_POS] & 0x7F;
    if (!in_vsync_ && vcc_ == vsync_pos && ra_ == 0) {
        in_vsync_ = true;
        vsync_counter_ = 0;
        active_start_set_ = false;  // reset: next VCC=0 is the active start
        // Timestamp for the monitor's vertical oscillator model: the
        // renderer needs the emulated time of each vsync leading edge.
        if (clk_cycles_)
            last_vsync_clk_ = *clk_cycles_;
    }
    if (in_vsync_) {
        vsync_counter_++;
        if (vsync_counter_ >= 16) {
            in_vsync_ = false;
            scanline_ = 0;
            return;
        }
    }

    // Record where active display starts (first VCC=0 after VSYNC).
    if (!active_start_set_ && vcc_ == 0 && ra_ == 0) {
        active_start_ = scanline_ + 1;  // next scanline will be stamped as VCC=0
        active_start_set_ = true;
    }

    // scanline_ advances unconditionally, wraps within buffer.
    // Clamped at FRAME_LINES-1 if VSYNC hasn't fired (shouldn't happen
    // in normal operation but prevents buffer overflow).
    if (++scanline_ >= FRAME_LINES)
        scanline_ = FRAME_LINES - 1;
}

void ISA_CGA::stamp_scanline() {
    ScanlineRegs& sr = scanline_regs_[scanline_];
    sr.mode        = mode_;
    sr.color       = color_;
    sr.ma          = ma_;
    sr.ra          = ra_;
    sr.vcc         = vcc_;
    sr.h_displayed = crtc_reg_[CRTC_HDISPLAYED];
    sr.v_displayed = crtc_reg_[CRTC_VDISPLAYED];
    sr.hsync_pos   = crtc_reg_[CRTC_HSYNC_POS];
    sr.hsync_width = crtc_reg_[CRTC_SYNC_WIDTH] & 0x0F;
    sr.h_total     = crtc_reg_[CRTC_HTOTAL];
    sr._pad[0] = sr._pad[1] = 0;

    // Capture the VRAM row the beam reads at this scanline.
    // The 6845 outputs MA (14-bit).  CGA maps MA to VRAM bytes:
    //   Text mode:     byte addr = (MA & 0x1FFF) * 2
    //   Graphics mode: byte addr = (MA & 0x0FFF) * 2 + (RA & 1) * 0x2000
    // Each MA = 2 VRAM bytes in both modes.  Row = h_displayed * 2 bytes.
    uint8_t h_disp = sr.h_displayed;
    if (h_disp == 0) h_disp = 80;
    uint32_t bytes = h_disp * 2;
    if (bytes > SCANLINE_ROW_BYTES) bytes = SCANLINE_ROW_BYTES;

    uint32_t vram_offset;
    if (mode_ & MODE_GRAPHICS) {
        vram_offset = ((ma_ & 0x0FFF) << 1) + (ra_ & 1) * 0x2000;
    } else {
        vram_offset = (ma_ & 0x1FFF) * 2;
    }

    // Copy with wrap within 16KB VRAM.
    auto* dst = reinterpret_cast<uint8_t*>(sr.vram_row);
    for (uint32_t i = 0; i < bytes; ++i)
        dst[i] = vram_[(vram_offset + i) & (FB_SIZE - 1)];
    // Zero remainder.
    for (uint32_t i = bytes; i < SCANLINE_ROW_BYTES; ++i)
        dst[i] = 0;

    // Apply snow: CPU accesses during this scanline stole CRTC fetches.
    // The corrupted cell shows the CPU's byte for one scanline; the row
    // refetches clean next scanline (and this slot is re-stamped next
    // frame), so the glitch flickers like real snow.
    for (int i = 0; i < snow_event_count_; ++i) {
        uint32_t off = (uint32_t)snow_events_[i].col * 2 + snow_events_[i].attr_half;
        if (off < bytes)
            dst[off] = snow_events_[i].byte;
    }
    snow_event_count_ = 0;
}

} // namespace bench
