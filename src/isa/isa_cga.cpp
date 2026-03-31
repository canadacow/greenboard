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
    clk_counter_ = 0;
    scanline_ = 0;
    vcc_ = 0;
    ra_ = 0;
    ma_ = 0;
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
            static constexpr uint32_t H_ACTIVE_CLK = 213;  // 640 dots / 3
            static constexpr uint32_t V_ACTIVE     = 200;
            static constexpr uint32_t V_SYNC_START = 224;
            static constexpr uint32_t V_SYNC_END   = 227;

            uint8_t status = 0;
            if (clk_counter_ >= H_ACTIVE_CLK || scanline_ >= V_ACTIVE)
                status |= 0x01;
            if (scanline_ >= V_SYNC_START && scanline_ < V_SYNC_END)
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
    return vram_[(addr - FB_BASE) & (FB_SIZE - 1)];
}

void ISA_CGA::on_mmio_write(uint32_t addr, uint8_t val) {
    vram_[(addr - FB_BASE) & (FB_SIZE - 1)] = val;
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
    if (++clk_counter_ >= CLK_PER_LINE) {
        clk_counter_ = 0;

        // Stamp the current scanline with registers + counters.
        stamp_scanline();

        // Tick 6845 counters for next scanline.
        uint8_t max_sl = crtc_reg_[CRTC_MAX_SCANLINE] & 0x1F;
        uint8_t h_disp = crtc_reg_[CRTC_HDISPLAYED];
        if (h_disp == 0) h_disp = 80;

        if (ra_ >= max_sl) {
            ra_ = 0;
            vcc_++;
            ma_ += h_disp;
        } else {
            ra_++;
        }

        // Advance scanline. On frame wrap, reset counters per 6845:
        // VCC=0, RA=0, MA reloaded from start_addr (R12:R13).
        if (++scanline_ >= FRAME_LINES) {
            scanline_ = 0;
            vcc_ = 0;
            ra_ = 0;
            ma_ = (crtc_reg_[CRTC_START_ADDR_H] << 8) | crtc_reg_[CRTC_START_ADDR_L];
        }
    }
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
    sr._pad[0] = sr._pad[1] = sr._pad[2] = 0;

    // Capture the VRAM row the beam reads at this scanline.
    // Text mode: MA addresses char+attr pairs, 2 bytes each.
    //   Copy h_displayed * 2 bytes starting at (MA & 0x1FFF) * 2.
    // Graphics mode: MA addresses bytes, RA0 selects bank.
    //   Copy from ((MA & 0x0FFF) << 1) + (RA & 1) * 0x2000.
    uint8_t h_disp = sr.h_displayed;
    if (h_disp == 0) h_disp = 80;
    uint32_t bytes = (mode_ & MODE_GRAPHICS) ? h_disp : h_disp * 2;
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
}

} // namespace bench
