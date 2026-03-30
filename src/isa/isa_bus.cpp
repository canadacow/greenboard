#include "isa/isa_bus.h"
#include <spdlog/spdlog.h>

namespace bench {

ISA_Bus::ISA_Bus() : CallbackComponent("ISA-Bus") { set_description("ISA Expansion Bus"); }

void ISA_Bus::install(IsaSlot& slot) {
    // Wire to bus signals from any slot (all slots share the same signals).
    for (int i = 0; i < 8; ++i) {
        if (slot.sd[i]) {
            sd_[i] = slot.sd[i]->pin();
            slot.sd[i]->connect(this);
        }
    }
    for (int i = 0; i < 20; ++i) {
        if (slot.sa[i])
            sa_[i] = slot.sa[i]->pin();
    }
    if (slot.ior) { ior_ = slot.ior->pin(); slot.ior->connect(this); }
    if (slot.iow) { iow_ = slot.iow->pin(); slot.iow->connect(this); }
    if (slot.memr) { memr_ = slot.memr->pin(); slot.memr->connect(this); }
    if (slot.memw) { memw_ = slot.memw->pin(); slot.memw->connect(this); }
    if (slot.aen) { aen_ = slot.aen->pin(); slot.aen->connect(this); }

    // DMA channels 1-3
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

    // T/C
    if (slot.tc) { tc_ = slot.tc->pin(); slot.tc->connect(this); }

    // IRQ lines 2-7
    Signal* irq_sigs[] = { nullptr, nullptr, slot.irq2, slot.irq3,
                           slot.irq4, slot.irq5, slot.irq6, slot.irq7 };
    for (int i = 2; i < 8; ++i) {
        irq_sig_[i] = irq_sigs[i];
        if (irq_sig_[i])
            irq_pin_[i] = irq_sig_[i]->pin();
    }

    // DAG declarations -- one set for the entire bus.
    declare_async_input(aen_);
    declare_bidir_block({sa_[0], sa_[1], sa_[2], sa_[3]},
        BidirDir::Input | BidirDir::HiZ,
        [this]() {
            return (dma_active() || aen_.level() == Level::High)
                   ? BidirDir::HiZ : BidirDir::Input;
        });
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

    // SD: one bidir block for the entire bus.
    declare_bidir_block(
        {sd_[0], sd_[1], sd_[2], sd_[3], sd_[4], sd_[5], sd_[6], sd_[7]},
        BidirDir::HiZ | BidirDir::Input | BidirDir::Output,
        [this]() -> BidirDir {
            // DMA: direction depends on transfer type.
            if (dma_active()) {
                for (int ch = 1; ch <= 3; ++ch)
                    if (dack_[ch].level() == Level::Low && ch == dma_active_ch_)
                        return dma_write_mode_ ? BidirDir::Input : BidirDir::Output;
            }
            auto ior_lev = ior_.level();
            if (ior_lev == Level::Low) {
                uint16_t port = static_cast<uint16_t>(SignalPool::bus_address);
                if (find_port_owner(port))
                    return BidirDir::Output;
                return BidirDir::HiZ;
            }
            if (iow_.level() == Level::Low) return BidirDir::Input;
            uint32_t addr = SignalPool::bus_address;
            if (memr_.level() == Level::Low && find_mmio_owner(addr))
                return BidirDir::Output;
            if (memw_.level() == Level::Low && find_mmio_owner(addr))
                return BidirDir::Input;
            return BidirDir::HiZ;
        });
}

void ISA_Bus::insert_card(int slot_idx, ISA_Card* card,
                          uint8_t dma_ch_mask, uint8_t irq_mask) {
    if (slot_idx < 0 || slot_idx >= MAX_SLOTS) return;
    cards_[slot_idx] = card;
    card->bus_ = this;
    if (slot_idx >= card_count_)
        card_count_ = slot_idx + 1;

    // Populate O(1) lookup tables.
    for (int p = 0; p < IO_PORTS; ++p)
        if (card->claims_port(static_cast<uint16_t>(p)))
            port_map_[p] = card;
    for (int pg = 0; pg < MMIO_PAGES; ++pg) {
        uint32_t base = static_cast<uint32_t>(pg) << 12;
        if (card->claims_mmio(base))
            mmio_map_[pg] = card;
    }

    // Map DMA channels to this card.
    for (int ch = 1; ch <= 3; ++ch)
        if (dma_ch_mask & (1 << ch))
            dma_owner_[ch] = card;
}

void ISA_Bus::on_power_on() {
    ior_prev_ = Level::HiZ;
    iow_prev_ = Level::HiZ;
    dma_memr_prev_ = Level::HiZ;
    dma_memw_prev_ = Level::HiZ;
    cpu_memr_prev_ = Level::HiZ;
    cpu_memw_prev_ = Level::HiZ;
    for (int ch = 0; ch < 4; ++ch)
        dack_prev_[ch] = Level::HiZ;
    tc_prev_ = Level::HiZ;
    data_driven_ = false;
    read_byte_ = 0;
    write_pending_ = false;
    read_pending_ = false;
    mem_write_pending_ = false;
    dma_active_ch_ = -1;
    dma_write_mode_ = false;
    dma_active_card_ = nullptr;
    dma_ior_count_ = 0;

    // Power on all inserted cards.
    for (int i = 0; i < card_count_; ++i)
        if (cards_[i])
            cards_[i]->on_power_on();
}

// =========================================================================
// Bus protocol -- one evaluation per cycle for all cards
// =========================================================================

void ISA_Bus::on_cycle(Fiber /*caller*/) {
    Level ior_cur = ior_.level();
    Level iow_cur = iow_.level();
    Level tc_cur = tc_.level();

    // --- DMA ---
    if (!dma_write_mode_) {
        // DMA READ (device -> memory): active card drives SD.
        if (dma_dack_pending_ && dma_active_card_) {
            dma_dack_pending_ = false;
            uint8_t byte = dma_active_card_->on_dma_read();
            drive_sd(byte);
            dma_ior_count_ = 0;
        }
        for (int ch = 1; ch <= 3; ++ch) {
            Level dack_cur = dack_[ch].level();
            if (dack_cur == Level::Low && dack_prev_[ch] != Level::Low && dma_active_ch_ == ch)
                dma_dack_pending_ = true;
            if (dack_cur != Level::Low && dack_prev_[ch] == Level::Low)
                release_sd();
            dack_prev_[ch] = dack_cur;
        }
        if (dma_active() && dma_active_card_ && ior_cur == Level::Low && ior_prev_ != Level::Low) {
            int ch = dma_active_ch_;
            if (dack_[ch].level() == Level::Low && dack_prev_[ch] == Level::Low) {
                if (dma_ior_count_ > 0) {
                    uint8_t byte = dma_active_card_->on_dma_read();
                    drive_sd(byte);
                }
                dma_ior_count_++;
            }
        }
    } else {
        // DMA WRITE (memory -> device): active card reads SD.
        for (int ch = 1; ch <= 3; ++ch)
            dack_prev_[ch] = dack_[ch].level();
        if (dma_dack_pending_ && dma_active_card_) {
            dma_dack_pending_ = false;
            uint8_t byte = read_sd();
            dma_active_card_->on_dma_write(byte);
        }
        if (dma_active() && iow_cur == Level::Low && iow_prev_ != Level::Low) {
            int ch = dma_active_ch_;
            if (dack_[ch].level() == Level::Low)
                dma_dack_pending_ = true;
        }
    }

    // T/C pulse: dispatch to the active card's channel.
    if (tc_cur != tc_prev_ && tc_prev_ != Level::HiZ && dma_active() && dma_active_card_) {
        int ch = dma_active_ch_;
        ISA_Card* card = dma_active_card_;
        dma_active_ch_ = -1;
        dma_active_card_ = nullptr;
        if (ch >= 1 && ch <= 3 && drq_sig_[ch])
            drq_sig_[ch]->drive(Level::Low);
        card->on_dma_complete(ch);
    }
    if (dma_active())
        tc_prev_ = tc_cur;

    // --- DMA MMIO: route DMA transfers to/from ISA expansion RAM ---
    // During DMA, AEN is High so the CPU I/O/MMIO block below is skipped.
    // But DMA memory writes (~MEMW) to expansion RAM must still reach the card,
    // and DMA memory reads (~MEMR) from expansion RAM must drive SD.
    if (dma_active()) {
        Level memr_cur = memr_.level();
        Level memw_cur = memw_.level();
        if (memw_cur == Level::Low && dma_memw_prev_ != Level::Low) {
            uint32_t addr = SignalPool::bus_address;
            ISA_Card* card = find_mmio_owner(addr);
            if (card)
                card->on_mmio_write(addr, read_sd());
        }
        if (memr_cur == Level::Low && dma_memr_prev_ != Level::Low) {
            uint32_t addr = SignalPool::bus_address;
            ISA_Card* card = find_mmio_owner(addr);
            if (card)
                drive_sd(card->on_mmio_read(addr));
        } else if (memr_cur != Level::Low && dma_memr_prev_ == Level::Low) {
            // Only release if we were driving for MMIO (not device DMA)
            uint32_t addr = SignalPool::bus_address;
            if (find_mmio_owner(addr) && data_driven_)
                release_sd();
        }
        dma_memr_prev_ = memr_cur;
        dma_memw_prev_ = memw_cur;
    }

    // --- CPU I/O ---
    bool addr_readable = aen_.level() != Level::High;
    if (addr_readable) {
        if (write_pending_) {
            uint16_t port = static_cast<uint16_t>(read_address());
            uint8_t val = read_sd();
            ISA_Card* card = find_port_owner(port);
            if (card)
                card->on_io_write(port, val);
            write_pending_ = false;
        } else if (iow_cur == Level::Low && iow_prev_ != Level::Low) {
            write_pending_ = true;
        }

        if (read_pending_) {
            bool any_dack = false;
            for (int ch = 1; ch <= 3; ++ch)
                if (dack_[ch].level() == Level::Low) any_dack = true;
            if (!any_dack) {
                uint16_t port = static_cast<uint16_t>(read_address());
                ISA_Card* card = find_port_owner(port);
                if (card) {
                    read_byte_ = card->on_io_read(port);
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
    }
    iow_prev_ = iow_cur;
    ior_prev_ = ior_cur;

    // --- CPU MMIO ---
    // Runs unconditionally: the 8288 bus recovery re-asserts ~MEMW/~MEMR
    // after DMA releases, and AEN may still be High at that point.
    // The CPU's address and data are still valid on the bus.
    {
        Level memr_cur = memr_.level();
        Level memw_cur = memw_.level();

        // Level-based MMIO write: while ~MEMW is Low and AEN is Low
        // (CPU owns bus), continuously write to the MMIO target.
        // This replaces edge-based detection which fails after DMA
        // because the edge trackers get polluted by DMA's ~MEMW pulses.
        if (memw_cur == Level::Low && aen_.level() != Level::High) {
            uint32_t addr = SignalPool::bus_address;
            ISA_Card* card = find_mmio_owner(addr);
            if (card) {
                uint8_t val = read_sd();
                //if (addr >= 0x40000 && addr < 0xA0000)
                //    spdlog::info("[ISA] MMIO level-write {:05X}={:02X}", addr, val);
                card->on_mmio_write(addr, val);
            }
        }

        if (memr_cur == Level::Low) {
            uint32_t addr = SignalPool::bus_address;
            ISA_Card* card = find_mmio_owner(addr);
            if (addr >= 0x40000 && addr < 0xA0000) {
                uint8_t val = card->on_mmio_read(addr);
                //spdlog::info("[ISA] MMIO read {:05X} val={:02X}", addr, val);
                drive_sd(val);
            } else if (card) {
                drive_sd(card->on_mmio_read(addr));
            }
        }
    }
}

// =========================================================================
// Bus helpers (called by cards)
// =========================================================================

void ISA_Bus::raise_irq(int n) {
    if (n >= 0 && n < 8 && irq_sig_[n])
        irq_sig_[n]->drive(Level::High);
}

void ISA_Bus::lower_irq(int n) {
    if (n >= 0 && n < 8 && irq_sig_[n])
        irq_sig_[n]->drive(Level::Low);
}

void ISA_Bus::assert_drq(int ch) {
    if (ch >= 1 && ch <= 3) {
        dma_active_ch_ = ch;
        dma_write_mode_ = false;
        dma_active_card_ = dma_owner_[ch];
        tc_prev_ = tc_.level();
        if (drq_sig_[ch])
            drq_sig_[ch]->drive(Level::High);
    }
}

void ISA_Bus::assert_drq_write(int ch) {
    if (ch >= 1 && ch <= 3) {
        dma_active_ch_ = ch;
        dma_write_mode_ = true;
        dma_active_card_ = dma_owner_[ch];
        tc_prev_ = tc_.level();
        if (drq_sig_[ch])
            drq_sig_[ch]->drive(Level::High);
    }
}

void ISA_Bus::deassert_drq(int ch) {
    if (ch >= 1 && ch <= 3) {
        dma_active_ch_ = -1;
        dma_active_card_ = nullptr;
        if (drq_sig_[ch])
            drq_sig_[ch]->drive(Level::Low);
    }
}

// =========================================================================
// Internal helpers
// =========================================================================

uint32_t ISA_Bus::read_address() {
    return SignalPool::bus_address;
}

uint8_t ISA_Bus::read_sd() {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i)
        if (sd_[i].level() == Level::High)
            val |= (1u << i);
    return val;
}

void ISA_Bus::drive_sd(uint8_t val) {
    for (int i = 0; i < 8; ++i)
        sd_[i].drive((val >> i) & 1 ? Level::High : Level::Low);
    data_driven_ = true;
}

void ISA_Bus::release_sd() {
    if (data_driven_) {
        for (int i = 0; i < 8; ++i)
            sd_[i].release();
        data_driven_ = false;
    }
}

// find_port_owner / find_mmio_owner are now inline O(1) lookups in isa_bus.h.

} // namespace bench
