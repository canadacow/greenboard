#include "isa/isa_mda.h"
#include <cstring>
#include <spdlog/spdlog.h>

namespace bench {

ISA_MDA::ISA_MDA() : ISA_Adapter("MDA") {}

void ISA_MDA::on_power_on() {
    ISA_Adapter::on_power_on();
    std::memset(fb_, 0, FB_SIZE);
}

bool ISA_MDA::claims_mmio(uint32_t addr) {
    return addr >= FB_BASE && addr < FB_BASE + FB_SIZE;
}

uint8_t ISA_MDA::on_mmio_read(uint32_t addr) {
    return fb_[addr - FB_BASE];
}

void ISA_MDA::on_mmio_write(uint32_t addr, uint8_t val) {
    uint32_t offset = addr - FB_BASE;
    uint8_t old = fb_[offset];
    fb_[offset] = val;

    // Log character writes (even offsets = character, odd = attribute).
    if ((offset & 1) == 0 && val != old && val >= 0x20 && val < 0x7F) {
        int col = (offset / 2) % 80;
        int row = (offset / 2) / 80;
        spdlog::info("[MDA] char '{}' at row={} col={}", (char)val, row, col);
    }
}

} // namespace bench
