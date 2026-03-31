#include "isa/isa_cga.h"
#include <cstring>
#include <fstream>
#include <spdlog/spdlog.h>

namespace bench {

ISA_CGA::ISA_CGA() {
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
    last_frame_num_ = UINT64_MAX;
    snapshot_from_scanline(0);
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
    check_frame_boundary();
    switch (port) {
        // CRTC index (mirrored at even ports)
        case 0x3D0: case 0x3D2: case 0x3D4: case 0x3D6:
            return crtc_index_;

        // CRTC data (mirrored at odd ports)
        case 0x3D1: case 0x3D3: case 0x3D5: case 0x3D7:
            if (crtc_index_ < 18)
                return crtc_reg_[crtc_index_];
            return 0;

        // Mode control register (write-only per spec, return 0)
        case 0x3D8:
            return 0;

        // Color select register (write-only per spec, return 0)
        case 0x3D9:
            return 0;

        // Status register -- CGA retrace timing derived from CLK cycles.
        // CGA: 912 dots/line, 262 lines/frame. CLK = 4.77 MHz = OSC/3.
        // 1 CLK = 3 dots. 304 CLK/line, 79648 CLK/frame.
        // Display active: 640 dots = 213 CLK. H-retrace: 272 dots = 91 CLK.
        // V-display: 200 lines. V-retrace: lines 224-226.
        case 0x3DA: {
            static constexpr uint32_t H_ACTIVE_CLK  = 213;  // 640 dots / 3
            static constexpr uint32_t V_ACTIVE      = 200;
            static constexpr uint32_t V_SYNC_START  = 224;
            static constexpr uint32_t V_SYNC_END    = 227;

            uint64_t clk = clk_cycles_ ? *clk_cycles_ : 0;
            uint32_t pos_in_frame = static_cast<uint32_t>(clk % CLK_PER_FRAME);
            uint32_t line = pos_in_frame / CLK_PER_LINE;
            uint32_t pos_in_line = pos_in_frame % CLK_PER_LINE;

            uint8_t status = 0;
            // Bit 0: display enable (1 = not in active display, safe for VRAM)
            if (pos_in_line >= H_ACTIVE_CLK || line >= V_ACTIVE)
                status |= 0x01;
            // Bit 3: vertical retrace
            if (line >= V_SYNC_START && line < V_SYNC_END)
                status |= 0x08;
            return status;
        }

        // Light pen (not implemented)
        case 0x3DB: case 0x3DC:
            return 0;

        default:
            return 0xFF;
    }
}

// =========================================================================
// I/O writes
// =========================================================================

void ISA_CGA::on_io_write(uint16_t port, uint8_t val) {
    check_frame_boundary();
    switch (port) {
        case 0x3D0: case 0x3D2: case 0x3D4: case 0x3D6:
            crtc_index_ = val & 0x1F;
            break;

        case 0x3D1: case 0x3D3: case 0x3D5: case 0x3D7:
            if (crtc_index_ < 18) {
                // MC6845 register masking per DOSBox vga_other.cpp:
                //   R4 (vtotal): 7 bits. R6 (vdend): 7 bits.
                //   R9 (max_scanline): 5 bits. R10 (cursor_start): 6 bits.
                //   R11 (cursor_end): 5 bits. R12 (start_addr_h): 6 bits.
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
                if (crtc_index_ == CRTC_START_ADDR_H ||
                    crtc_index_ == CRTC_START_ADDR_L) {
                    uint32_t sl = current_scanline();
                    if (sl < FRAME_LINES) snapshot_from_scanline(sl);
                }
            }
            break;

        case 0x3D8:
            mode_ = val;
            { uint32_t sl = current_scanline();
              if (sl < FRAME_LINES) snapshot_from_scanline(sl); }
            break;

        case 0x3D9:
            color_ = val;
            { uint32_t sl = current_scanline();
              if (sl < FRAME_LINES) snapshot_from_scanline(sl); }
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
    return vram_[(addr - FB_BASE) & (FB_SIZE - 1)];
}

void ISA_CGA::on_mmio_write(uint32_t addr, uint8_t val) {
    uint32_t off = (addr - FB_BASE) & (FB_SIZE - 1);
    vram_[off] = val;
}

// =========================================================================
// GPU constant buffer fill
// =========================================================================

void ISA_CGA::fill_gpu_constants(GpuConstants& cb) const {
    cb.mode = mode_;
    cb.color = color_;

    // Compute blink from wall clock at CGA frame rate (~59.92 Hz).
    // 5-bit frame counter: cursor blinks at bit 3 (1/16), attr at bit 4 (1/32).
    LARGE_INTEGER qpc_now;
    QueryPerformanceCounter(&qpc_now);
    double elapsed = double(qpc_now.QuadPart - qpc_start_.QuadPart) / qpc_freq_.QuadPart;
    uint32_t frames = static_cast<uint32_t>(elapsed * CGA_FRAME_HZ);
    uint8_t counter = frames & 0x1F;
    cb.cursor_blink = (counter & 0x08) ? 1 : 0;  // bit 3: ~3.75 Hz
    cb.attr_blink = (counter & 0x10) ? 0 : 1;    // bit 4: ~1.875 Hz (inverted: visible when 0)

    cb.composite = composite_ ? 1 : 0;
    cb.start_addr = (crtc_reg_[CRTC_START_ADDR_H] << 8) | crtc_reg_[CRTC_START_ADDR_L];
    cb.cursor_addr = (crtc_reg_[CRTC_CURSOR_H] << 8) | crtc_reg_[CRTC_CURSOR_L];
    cb.cursor_start = crtc_reg_[CRTC_CURSOR_START] & 0x1F;
    cb.cursor_end = crtc_reg_[CRTC_CURSOR_END] & 0x1F;
    // DOSBox: cursor enabled = ((val & 0x60) != 0x20)
    // Bits 5:6 of cursor_start: 00=on, 01=off, 10=blink/16, 11=blink/32
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
// Per-scanline beam-racing support
// =========================================================================

uint32_t ISA_CGA::current_scanline() const {
    if (!clk_cycles_) return 0;
    uint32_t pos = static_cast<uint32_t>(*clk_cycles_ % CLK_PER_FRAME);
    return pos / CLK_PER_LINE;
}

void ISA_CGA::snapshot_from_scanline(uint32_t from) {
    uint16_t sa = (crtc_reg_[CRTC_START_ADDR_H] << 8) | crtc_reg_[CRTC_START_ADDR_L];
    ScanlineRegs s = { mode_, color_, sa, 0 };
    for (uint32_t i = from; i < FRAME_LINES; ++i)
        scanline_regs_[i] = s;
}

void ISA_CGA::check_frame_boundary() {
    if (!clk_cycles_) return;
    uint64_t frame = *clk_cycles_ / CLK_PER_FRAME;
    if (frame != last_frame_num_) {
        last_frame_num_ = frame;
        snapshot_from_scanline(0);
    }
}

} // namespace bench
