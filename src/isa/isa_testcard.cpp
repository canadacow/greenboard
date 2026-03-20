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

    // Control: ~IOR, ~IOW, ~MEMR, ~MEMW (subscribe for edge detection).
    if (slot.ior) { ior_ = slot.ior->pin(); slot.ior->connect(this); }
    if (slot.iow) { iow_ = slot.iow->pin(); slot.iow->connect(this); }
    if (slot.memr) { memr_ = slot.memr->pin(); slot.memr->connect(this); }
    if (slot.memw) { memw_ = slot.memw->pin(); slot.memw->connect(this); }

    // DMA channels 1-3: ~DACKn (input), DRQn (output).
    Signal* dack_sigs[] = { slot.dack0, slot.dack1, slot.dack2, slot.dack3 };
    Signal* drq_sigs[]  = { nullptr,    slot.drq1,  slot.drq2,  slot.drq3  };
    for (int ch = 1; ch <= 3; ++ch) {
        if (dack_sigs[ch]) {
            dack_[ch] = dack_sigs[ch]->pin();
            dack_sigs[ch]->connect(this);
        }
        drq_sig_[ch] = drq_sigs[ch];
        if (drq_sig_[ch])
            drq_[ch] = drq_sig_[ch]->pin();
    }

    // T/C (terminal count): rising edge = DMA transfer complete.
    if (slot.tc) { tc_ = slot.tc->pin(); slot.tc->connect(this); }

    // IRQ lines: ISA slot provides IRQ2-IRQ7.
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
    // SA0-SA3: HiZ during DMA to break DAG cycle.
    declare_bidir_block({sa_[0], sa_[1], sa_[2], sa_[3]},
        BidirDir::Input | BidirDir::HiZ,
        [this]() { return dma_active() ? BidirDir::HiZ : BidirDir::Input; });
    for (int i = 4; i < 20; ++i) declare_input(sa_[i]);
    declare_input(ior_);
    declare_input(iow_);
    declare_input(memr_);
    declare_input(memw_);
    for (int ch = 1; ch <= 3; ++ch)
        declare_async_input(dack_[ch]);
    declare_async_input(tc_);
    for (int i = 2; i < 8; ++i) {
        if (irq_sig_[i])
            declare_output(irq_pin_[i]);
    }
    for (int ch = 1; ch <= 3; ++ch) {
        if (drq_sig_[ch])
            declare_output(drq_[ch]);
    }

    // SD is bidirectional: output during reads, input during writes,
    // output during DMA transfers.
    declare_bidir_block(
        {sd_[0], sd_[1], sd_[2], sd_[3], sd_[4], sd_[5], sd_[6], sd_[7]},
        BidirDir::HiZ | BidirDir::Input | BidirDir::Output,
        [this]() -> BidirDir {
            // DMA: card drives data when any ~DACKn is asserted.
            if (dma_active()) {
                for (int ch = 1; ch <= 3; ++ch)
                    if (dack_[ch].level() == Level::Low && ch == dma_active_ch_)
                        return BidirDir::Output;
            }
            auto ior_lev = ior_.level();
            auto iow_lev = iow_.level();
            if (ior_lev == Level::Low) {
                uint16_t port = static_cast<uint16_t>(read_address());
                if (my_port(port)) {
                    spdlog::trace("[{}] bidir: ~IOR={} port=0x{:04X} -> OUT",
                                  name(), int(ior_lev), port);
                    return BidirDir::Output;
                }
                return BidirDir::HiZ;
            }
            if (iow_lev == Level::Low) return BidirDir::Input;
            if (memr_.level() == Level::Low && my_mmio(read_address()))
                return BidirDir::Output;
            if (memw_.level() == Level::Low && my_mmio(read_address()))
                return BidirDir::Input;
            return BidirDir::HiZ;
        });
}

void ISA_TestCard::on_power_on() {
    ior_prev_ = Level::HiZ;
    iow_prev_ = Level::HiZ;
    for (int ch = 0; ch < 4; ++ch)
        dack_prev_[ch] = Level::HiZ;
    tc_prev_ = Level::HiZ;
    data_driven_ = false;
    read_byte_ = 0;
    write_pending_ = false;
    read_pending_ = false;
    mem_write_pending_ = false;
    memr_prev_ = Level::HiZ;
    memw_prev_ = Level::HiZ;
    dma_ptr_ = 0;
    dma_active_ch_ = -1;
    dma_ior_count_ = 0;
}

// =========================================================================
// Signal change handler
// =========================================================================

void ISA_TestCard::on_signal_change(Fiber /*caller*/) {
    Level ior_cur = ior_.level();
    Level iow_cur = iow_.level();
    Level tc_cur = tc_.level();

    // --- DMA channels 1-3 ---
    for (int ch = 1; ch <= 3; ++ch) {
        Level dack_cur = dack_[ch].level();
        // ~DACKn falling edge: drive first byte, reset IOR counter.
        if (dack_cur == Level::Low && dack_prev_[ch] != Level::Low && dma_active_ch_ == ch) {
            uint8_t byte = dma_buf_[dma_ptr_ % DMA_BUF_SIZE];
            spdlog::debug("[{}] DMA DACK{}: driving byte [{}]=0x{:02X}", name(), ch, dma_ptr_, byte);
            drive_sd(byte);
            dma_ptr_++;
            dma_ior_count_ = 0;
        }
        // ~DACKn rising edge: release data bus.
        if (dack_cur != Level::Low && dack_prev_[ch] == Level::Low) {
            release_sd();
        }
        dack_prev_[ch] = dack_cur;
    }

    // Block/demand mode: ~DACK stays Low, 8237A pulses ~IOR for each byte.
    // Skip the first IOR (overlaps with DACK-driven byte in single mode).
    // Subsequent IOR falling edges advance to the next byte.
    if (dma_active() && ior_cur == Level::Low && ior_prev_ != Level::Low) {
        int ch = dma_active_ch_;
        if (dack_[ch].level() == Level::Low) {
            if (dma_ior_count_ > 0) {
                uint8_t byte = dma_buf_[dma_ptr_ % DMA_BUF_SIZE];
                spdlog::debug("[{}] DMA IOR ch{}: driving byte [{}]=0x{:02X}", name(), ch, dma_ptr_, byte);
                drive_sd(byte);
                dma_ptr_++;
            }
            dma_ior_count_++;
        }
    }

    // T/C rising edge: DMA transfer complete. Deassert DRQn, fire IRQ.
    if (dma_active())
        spdlog::debug("[{}] T/C check: tc_cur={} tc_prev={} dma_ch={} tc_.idx={}",
                      name(), int(tc_cur), int(tc_prev_), dma_active_ch_, tc_.idx);
    if (tc_cur == Level::High && tc_prev_ != Level::High && dma_active()) {
        int ch = dma_active_ch_;
        spdlog::debug("[{}] DMA T/C: ch{} transfer complete, {} bytes sent, firing IRQ{}",
                      name(), ch, dma_ptr_, dma_irq_);
        dma_active_ch_ = -1;
        if (ch >= 1 && ch <= 3 && drq_sig_[ch])
            drq_sig_[ch]->drive(Level::Low);
        if (dma_irq_ >= 2 && dma_irq_ <= 7 && irq_sig_[dma_irq_])
            irq_sig_[dma_irq_]->drive(Level::High);
    }
    tc_prev_ = tc_cur;

    // --- CPU I/O ---
    if (write_pending_) {
        uint16_t port = static_cast<uint16_t>(read_address());
        uint8_t val = read_sd();
        spdlog::trace("[{}] IOW: port=0x{:04X} val=0x{:02X} my={}",
                      name(), port, val, my_port(port));
        if (my_port(port)) {
            io_write(port, val);
        }
        write_pending_ = false;
    } else if (iow_cur == Level::Low && iow_prev_ != Level::Low) {
        write_pending_ = true;
    }
    iow_prev_ = iow_cur;

    if (read_pending_) {
        bool any_dack = false;
        for (int ch = 1; ch <= 3; ++ch)
            if (dack_[ch].level() == Level::Low) any_dack = true;
        if (!any_dack) {
            uint16_t port = static_cast<uint16_t>(read_address());
            if (my_port(port)) {
                read_byte_ = io_read(port);
                spdlog::trace("[{}] READ port=0x{:04X} -> 0x{:02X}", name(), port, read_byte_);
                drive_sd(read_byte_);
            }
        }
        read_pending_ = false;
    } else if (ior_cur == Level::Low && ior_prev_ != Level::Low) {
        read_pending_ = true;
    } else if (!dma_active()) {
        if (ior_cur == Level::Low && data_driven_) {
            drive_sd(read_byte_);
        } else if (ior_cur != Level::Low && data_driven_) {
            release_sd();
        }
    }
    ior_prev_ = ior_cur;

    // --- MMIO (memory-mapped I/O at 0xB8000-0xBBFFF) ---
    Level memr_cur = memr_.level();
    Level memw_cur = memw_.level();

    if (mem_write_pending_) {
        uint32_t addr = read_address();
        if (my_mmio(addr)) {
            uint8_t val = read_sd();
            mmio_[addr - MMIO_BASE] = val;
            spdlog::trace("[{}] MMIO WRITE 0x{:05X} = 0x{:02X}", name(), addr, val);
        }
        mem_write_pending_ = false;
    } else if (memw_cur == Level::Low && memw_prev_ != Level::Low) {
        uint32_t addr = read_address();
        if (my_mmio(addr))
            mem_write_pending_ = true;
    }
    memw_prev_ = memw_cur;

    if (memr_cur == Level::Low) {
        // Drive data as long as ~MEMR is active (level-based, not edge).
        uint32_t addr = read_address();
        if (my_mmio(addr)) {
            uint8_t val = mmio_[addr - MMIO_BASE];
            if (!data_driven_ || memr_prev_ != Level::Low)
                spdlog::trace("[{}] MMIO READ 0x{:05X} = 0x{:02X}", name(), addr, val);
            drive_sd(val);
        }
    } else if (memr_prev_ == Level::Low) {
        // ~MEMR released -- stop driving.
        if (data_driven_) release_sd();
    }
    memr_prev_ = memr_cur;
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
        // DMA start on channel N (val = 1, 2, or 3).
        if (val >= 1 && val <= 3) {
            dma_ptr_ = 0;
            dma_active_ch_ = val;
            spdlog::debug("[{}] DMA start: asserting DRQ{}, ptr={}", name(), val, dma_ptr_);
            if (drq_sig_[val])
                drq_sig_[val]->drive(Level::High);
        }
    } else if (port == 0xF5) {
        // DMA stop on channel N.
        if (val >= 1 && val <= 3) {
            dma_active_ch_ = -1;
            if (drq_sig_[val])
                drq_sig_[val]->drive(Level::Low);
        }
    } else if (port == 0xF6) {
        // Set DMA completion IRQ number (2-7).
        if (val >= 2 && val <= 7)
            dma_irq_ = val;
    } else {
        io_[port] = val;
    }
}

} // namespace bench
