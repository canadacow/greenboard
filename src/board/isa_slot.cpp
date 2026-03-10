#include "board/isa_slot.h"
#include "core/component.h"
#include <spdlog/spdlog.h>

namespace bench {

IsaSlot::IsaSlot(std::string ref_designator) : ref_(std::move(ref_designator)) {}

void IsaSlot::wire_pin(int pin, Signal* sig) {
    // Component side: pins 1-31 (A1-A31)
    switch (pin) {
        case  1: io_ch_ck = sig; return;
        // A2-A9: SD7..SD0
        case  2: sd[7] = sig; return;
        case  3: sd[6] = sig; return;
        case  4: sd[5] = sig; return;
        case  5: sd[4] = sig; return;
        case  6: sd[3] = sig; return;
        case  7: sd[2] = sig; return;
        case  8: sd[1] = sig; return;
        case  9: sd[0] = sig; return;
        case 10: io_ch_rdy = sig; return;
        case 11: aen = sig; return;
        // A12-A31: SA19..SA0
        case 12: sa[19] = sig; return;
        case 13: sa[18] = sig; return;
        case 14: sa[17] = sig; return;
        case 15: sa[16] = sig; return;
        case 16: sa[15] = sig; return;
        case 17: sa[14] = sig; return;
        case 18: sa[13] = sig; return;
        case 19: sa[12] = sig; return;
        case 20: sa[11] = sig; return;
        case 21: sa[10] = sig; return;
        case 22: sa[9]  = sig; return;
        case 23: sa[8]  = sig; return;
        case 24: sa[7]  = sig; return;
        case 25: sa[6]  = sig; return;
        case 26: sa[5]  = sig; return;
        case 27: sa[4]  = sig; return;
        case 28: sa[3]  = sig; return;
        case 29: sa[2]  = sig; return;
        case 30: sa[1]  = sig; return;
        case 31: sa[0]  = sig; return;
        // Solder side: pins 32-62 (B1-B31)
        case 32: gnd_b1    = sig; return;
        case 33: reset_drv = sig; return;
        case 34: vcc_b3    = sig; return;
        case 35: irq2      = sig; return;
        case 36: vcc_n5    = sig; return;
        case 37: drq2      = sig; return;
        case 38: vcc_n12   = sig; return;
        case 39: reserved  = sig; return;
        case 40: vcc_12    = sig; return;
        case 41: gnd_b10   = sig; return;
        case 42: memw      = sig; return;
        case 43: memr      = sig; return;
        case 44: iow       = sig; return;
        case 45: ior       = sig; return;
        case 46: dack3     = sig; return;
        case 47: drq3      = sig; return;
        case 48: dack1     = sig; return;
        case 49: drq1      = sig; return;
        case 50: dack0     = sig; return;
        case 51: clk       = sig; return;
        case 52: irq7      = sig; return;
        case 53: irq6      = sig; return;
        case 54: irq5      = sig; return;
        case 55: irq4      = sig; return;
        case 56: irq3      = sig; return;
        case 57: dack2     = sig; return;
        case 58: tc        = sig; return;
        case 59: ale       = sig; return;
        case 60: vcc_b29   = sig; return;
        case 61: osc       = sig; return;
        case 62: gnd_b31   = sig; return;
        default: break;
    }
}

void IsaSlot::insert(std::unique_ptr<Component> card) {
    if (card_) {
        spdlog::warn("[{}] ejecting {} to insert new card", ref_, card_->name());
        eject();
    }
    spdlog::info("[{}] inserted card: {}", ref_, card->name());
    card_ = std::move(card);
}

std::unique_ptr<Component> IsaSlot::eject() {
    if (!card_) return nullptr;
    spdlog::info("[{}] ejected card: {}", ref_, card_->name());
    return std::move(card_);
}

} // namespace bench
