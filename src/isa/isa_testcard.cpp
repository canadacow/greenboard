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

    // DMA channel 1: ~DACK1 (input), DRQ1 (output).
    if (slot.dack1) { dack1_ = slot.dack1->pin(); slot.dack1->connect(this); }
    drq1_sig_ = slot.drq1;
    if (drq1_sig_) drq1_ = drq1_sig_->pin();

    // T/C (terminal count): rising edge = DMA transfer complete.
    if (slot.tc) { tc_ = slot.tc->pin(); slot.tc->connect(this); }

    // IRQ lines: ISA slot provides IRQ2-IRQ7.
    // IRQ0 and IRQ1 are motherboard-only (not on ISA bus).
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
    declare_async_input(dack1_);  // ~DACK1: cross-cycle (asserted by DMA controller)
    declare_async_input(tc_);    // T/C: cross-cycle pulse from DMA controller
    for (int i = 2; i < 8; ++i) {
        if (irq_sig_[i])
            declare_output(irq_pin_[i]);
    }
    if (drq1_sig_)
        declare_output(drq1_);

    // SD is bidirectional: output during reads (~IOR low AND our port), input during writes,
    // and output during DMA transfers (~DACK1 low).
    declare_bidir_block(
        {sd_[0], sd_[1], sd_[2], sd_[3], sd_[4], sd_[5], sd_[6], sd_[7]},
        BidirDir::HiZ | BidirDir::Input | BidirDir::Output,
        [this]() -> BidirDir {
            // DMA: card drives data when ~DACK1 is asserted (active low).
            if (dack1_.level() == Level::Low && dma_active_)
                return BidirDir::Output;
            auto ior_lev = ior_.level();
            auto iow_lev = iow_.level();
            if (ior_lev == Level::Low) {
                // Only claim bus if this is our port range.
                uint16_t port = static_cast<uint16_t>(read_address());
                if (my_port(port)) {
                    spdlog::trace("[{}] bidir: ~IOR={} port=0x{:04X} -> OUT",
                                  name(), int(ior_lev), port);
                    return BidirDir::Output;
                }
                return BidirDir::HiZ;
            }
            if (iow_lev == Level::Low) return BidirDir::Input;
            return BidirDir::HiZ;
        });
}

void ISA_TestCard::reset_state() {
    ior_prev_ = Level::HiZ;
    iow_prev_ = Level::HiZ;
    dack1_prev_ = Level::HiZ;
    tc_prev_ = Level::HiZ;
    data_driven_ = false;
    read_byte_ = 0;
    dma_ptr_ = 0;
    dma_active_ = false;
}

// =========================================================================
// Signal change handler
// =========================================================================

void ISA_TestCard::on_signal_change(Fiber /*caller*/) {
    Level ior_cur = ior_.level();
    Level iow_cur = iow_.level();
    Level dack1_cur = dack1_.level();
    Level tc_cur = tc_.level();

    // --- DMA channel 1 ---
    // ~DACK1 falling edge: DMA controller acknowledges our request.
    // Drive the next byte from dma_buf_ onto the data bus.
    if (dack1_cur == Level::Low && dack1_prev_ != Level::Low && dma_active_) {
        uint8_t byte = dma_buf_[dma_ptr_ % DMA_BUF_SIZE];
        spdlog::debug("[{}] DMA DACK1: driving byte [{}]=0x{:02X}", name(), dma_ptr_, byte);
        drive_sd(byte);
        dma_ptr_++;
    }
    // ~DACK1 rising edge: release data bus after DMA transfer cycle.
    if (dack1_cur != Level::Low && dack1_prev_ == Level::Low) {
        release_sd();
    }
    dack1_prev_ = dack1_cur;

    // T/C rising edge: DMA transfer complete. Deassert DRQ1, fire IRQ.
    if (dma_active_)
        spdlog::debug("[{}] T/C check: tc_cur={} tc_prev={} dma_active={} tc_.idx={}",
                      name(), int(tc_cur), int(tc_prev_), dma_active_, tc_.idx);
    if (tc_cur == Level::High && tc_prev_ != Level::High && dma_active_) {
        spdlog::debug("[{}] DMA T/C: transfer complete, {} bytes sent, firing IRQ{}",
                      name(), dma_ptr_, dma_irq_);
        dma_active_ = false;
        if (drq1_sig_)
            drq1_sig_->drive(Level::Low);
        // Fire completion IRQ.
        if (dma_irq_ >= 2 && dma_irq_ <= 7 && irq_sig_[dma_irq_])
            irq_sig_[dma_irq_]->drive(Level::High);
    }
    tc_prev_ = tc_cur;

    // --- CPU I/O ---
    // No idea what the fuck the AI was doing.
    if (iow_.level() == Level::Low) {
        uint16_t port = static_cast<uint16_t>(read_address());
        uint8_t val = read_sd();
        spdlog::trace("[{}] IOW rise: port=0x{:04X} val=0x{:02X} my={} sa7={}(idx={}) sa5={}(idx={}) sa0={}(idx={})",
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
    // Skip during DMA (~DACK1 active) -- DMA data drive handled above.
    if (dack1_cur != Level::Low) {
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
    } else if (port == 0xF4) {
        // DMA start: assert DRQ1.
        spdlog::debug("[{}] DMA start: asserting DRQ1, ptr={}", name(), dma_ptr_);
        dma_active_ = true;
        if (drq1_sig_)
            drq1_sig_->drive(Level::High);
    } else if (port == 0xF5) {
        // DMA stop: deassert DRQ1.
        dma_active_ = false;
        if (drq1_sig_)
            drq1_sig_->drive(Level::Low);
    } else if (port == 0xF6) {
        // Set DMA completion IRQ number (2-7).
        if (val >= 2 && val <= 7)
            dma_irq_ = val;
    } else {
        io_[port] = val;
    }
}

} // namespace bench
