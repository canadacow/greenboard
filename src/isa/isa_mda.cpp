#include "isa/isa_mda.h"
#include <cstring>
#include <spdlog/spdlog.h>

namespace bench {

ISA_MDA::ISA_MDA() : ISA_Adapter("MDA") {}

void ISA_MDA::on_power_on() {
    ISA_Adapter::on_power_on();
    std::memset(fb_, 0, FB_SIZE);
    std::memset(crtc_reg_, 0, sizeof(crtc_reg_));
    crtc_index_ = 0;
    mode_ = 0;
    status_counter_ = 0;
}

bool ISA_MDA::claims_port(uint16_t port) {
    return port >= 0x3B0 && port <= 0x3BB;
}

bool ISA_MDA::claims_mmio(uint32_t addr) {
    return addr >= FB_BASE && addr < FB_BASE + FB_SIZE;
}

uint8_t ISA_MDA::on_io_read(uint16_t port) {
    switch (port) {
        case 0x3B0:
        case 0x3B2:
        case 0x3B4:
        case 0x3B6:
            return crtc_index_;

        case 0x3B1:
        case 0x3B3:
        case 0x3B5:
        case 0x3B7:
            if (crtc_index_ < 18)
                return crtc_reg_[crtc_index_];
            return 0;

        case 0x3B8:
            return mode_;

        case 0x3BA: {
            // Status register.
            // Bit 0: horizontal retrace (toggles on each read)
            // Bit 3: video signal (toggles at lower rate)
            // The BIOS TEST.10 polls for on->off and off->on transitions.
            ++status_counter_;
            uint8_t status = 0;
            if (status_counter_ & 1)
                status |= 0x01;    // hsync
            if (status_counter_ & 4)
                status |= 0x08;    // video
            return status;
        }

        default:
            return 0xFF;
    }
}

void ISA_MDA::on_io_write(uint16_t port, uint8_t val) {
    switch (port) {
        case 0x3B0:
        case 0x3B2:
        case 0x3B4:
        case 0x3B6:
            crtc_index_ = val & 0x1F;
            break;

        case 0x3B1:
        case 0x3B3:
        case 0x3B5:
        case 0x3B7:
            if (crtc_index_ < 18)
                crtc_reg_[crtc_index_] = val;
            break;

        case 0x3B8:
            mode_ = val;
            break;

        default:
            break;
    }
}

uint8_t ISA_MDA::on_mmio_read(uint32_t addr) {
    return fb_[addr - FB_BASE];
}

void ISA_MDA::on_mmio_write(uint32_t addr, uint8_t val) {
    uint32_t offset = addr - FB_BASE;
    uint8_t old = fb_[offset];
    fb_[offset] = val;
}

} // namespace bench
