#include "isa/isa_testcard.h"
#include <cstring>
#include <spdlog/spdlog.h>

namespace bench {

ISA_TestCard::ISA_TestCard()
    : CallbackComponent("ISA-TestCard") {
    set_description("ISA Test Card");
}

void ISA_TestCard::install(IsaSlot& slot) {
    // Data bus: SD0-SD7 from ISA slot pins A2-A9 (already mapped by IsaSlot).
    for (int i = 0; i < 8; ++i) {
        if (slot.sd[i]) {
            sd_[i] = slot.sd[i]->pin();
            slot.sd[i]->connect(this);
        }
    }

    // Address bus: SA0-SA19.
    for (int i = 0; i < 20; ++i) {
        if (slot.sa[i]) {
            sa_[i] = slot.sa[i]->pin();
        }
    }

    // Control: ~IOR, ~IOW (subscribe for edge detection).
    if (slot.ior) { ior_ = slot.ior->pin(); slot.ior->connect(this); }
    if (slot.iow) { iow_ = slot.iow->pin(); slot.iow->connect(this); }

    // IRQ lines: ISA slot provides IRQ2-IRQ7.
    // IRQ0 and IRQ1 are motherboard-only (not on ISA bus).
    // Map: irq_sig_[2]=IRQ2, irq_sig_[3]=IRQ3, ..., irq_sig_[7]=IRQ7.
    irq_sig_[2] = slot.irq2;
    irq_sig_[3] = slot.irq3;
    irq_sig_[4] = slot.irq4;
    irq_sig_[5] = slot.irq5;
    irq_sig_[6] = slot.irq6;
    irq_sig_[7] = slot.irq7;
    for (int i = 2; i < 8; ++i) {
        if (irq_sig_[i])
            irq_pin_[i] = irq_sig_[i]->pin();
    }

    // Pin directions for DAG.
    for (int i = 0; i < 20; ++i) declare_input(sa_[i]);
    declare_input(ior_);
    declare_input(iow_);
    for (int i = 2; i < 8; ++i) {
        if (irq_sig_[i])
            declare_output(irq_pin_[i]);
    }

    // SD is bidirectional: output during reads (~IOR low), input during writes (~IOW low).
    declare_bidir_block(
        {sd_[0], sd_[1], sd_[2], sd_[3], sd_[4], sd_[5], sd_[6], sd_[7]},
        BidirDir::HiZ | BidirDir::Input | BidirDir::Output,
        [this]() -> BidirDir {
            auto ior_lev = ior_.level();
            auto iow_lev = iow_.level();
            if (ior_lev == Level::Low || iow_lev == Level::Low) {
                spdlog::trace("[{}] bidir: ~IOR={} ~IOW={} idx_ior={} idx_iow={}",
                              name(), int(ior_lev), int(iow_lev), ior_.idx, iow_.idx);
            }
            if (ior_lev == Level::Low) return BidirDir::Output;
            if (iow_lev == Level::Low) return BidirDir::Input;
            return BidirDir::HiZ;
        });
}

void ISA_TestCard::reset_state() {
    ior_prev_ = Level::HiZ;
    iow_prev_ = Level::HiZ;
    data_driven_ = false;
    read_byte_ = 0;
}

// =========================================================================
// Signal change handler
// =========================================================================

void ISA_TestCard::on_signal_change(Fiber /*caller*/) {
    Level ior_cur = ior_.level();
    Level iow_cur = iow_.level();

    // ~IOW falling edge: CPU writes to I/O port.
    if (iow_cur == Level::Low && iow_prev_ != Level::Low) {
        uint16_t port = static_cast<uint16_t>(read_address());
        uint8_t val = read_sd();
        spdlog::trace("[{}] IOW edge: port=0x{:04X} val=0x{:02X} my={} sa7={}(idx={}) sa5={}(idx={}) sa0={}(idx={})",
                      name(), port, val, my_port(port),
                      int(sa_[7].level()), sa_[7].idx,
                      int(sa_[5].level()), sa_[5].idx,
                      int(sa_[0].level()), sa_[0].idx);
        if (my_port(port)) {
            io_write(port, val);
        }
    }
    iow_prev_ = iow_cur;

    // ~IOR: level-sensitive data drive (same pattern as PIT).
    // Drive data while ~IOR is low, release when it goes high.
    if (ior_cur == Level::Low) {
        if (!data_driven_) {
            // First cycle: latch address and look up I/O value.
            uint16_t port = static_cast<uint16_t>(read_address());
            if (my_port(port)) {
                read_byte_ = io_read(port);
                spdlog::trace("[{}] READ port=0x{:04X} -> 0x{:02X}", name(), port, read_byte_);
                drive_sd(read_byte_);
            }
        } else {
            // Re-drive same value (bus hold).
            drive_sd(read_byte_);
        }
    } else if (data_driven_) {
        release_sd();
    }
    ior_prev_ = ior_cur;
}

// =========================================================================
// Bus helpers
// =========================================================================

uint32_t ISA_TestCard::read_address() {
    uint32_t addr = 0;
    for (int i = 0; i < 20; ++i)
        if (sa_[i].level() == Level::High)
            addr |= (1u << i);
    return addr;
}

uint8_t ISA_TestCard::read_sd() {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i)
        if (sd_[i].level() == Level::High)
            val |= (1u << i);
    return val;
}

void ISA_TestCard::drive_sd(uint8_t val) {
    for (int i = 0; i < 8; ++i)
        sd_[i].drive((val >> i) & 1 ? Level::High : Level::Low);
    data_driven_ = true;
}

void ISA_TestCard::release_sd() {
    if (data_driven_) {
        for (int i = 0; i < 8; ++i)
            sd_[i].release();
        data_driven_ = false;
    }
}

// =========================================================================
// I/O handlers
// =========================================================================

uint8_t ISA_TestCard::io_read(uint16_t port) {
    return io_[port];
}

void ISA_TestCard::io_write(uint16_t port, uint8_t val) {
    if (port == 0xF0) {
        // Test trigger: drive IRQ lines High.
        for (int i = 0; i < 8; i++) {
            if ((val & (1 << i)) && irq_sig_[i])
                irq_sig_[i]->drive(Level::High);
        }
    } else if (port == 0xF1) {
        // Test clear: drive IRQ lines Low.
        for (int i = 0; i < 8; i++) {
            if ((val & (1 << i)) && irq_sig_[i])
                irq_sig_[i]->drive(Level::Low);
        }
    } else {
        io_[port] = val;
    }
}

} // namespace bench
