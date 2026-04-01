#include "ic/ic_8253.h"
#include "audio/pc_speaker.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8253::IC_8253() : CallbackComponent("8253") { set_description("PIT"); }

void IC_8253::on_power_on() {
    for (int i = 0; i < 3; ++i)
        channels_[i] = Channel{};
    data_bus_driven_ = false;
    write_pending_ = false;
    read_pending_ = false;
    pit_timer_ = 0;

    // Drive output pins to match reset state.
    for (int i = 0; i < 3; ++i)
        update_out(i);
}

void IC_8253::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    // Data bus D0-D7 (active during ~CS + ~RD or ~WR).
    // Pin 8=D0, Pin 7=D1, ... Pin 1=D7.
    for (int i = 0; i < 8; ++i)
        pin_data_[i] = pin(8 - i);

    // Counter clock inputs.
    pin_clk_[0] = connect_pin(9);    // CLK0
    pin_clk_[1] = connect_pin(15);   // CLK1
    pin_clk_[2] = connect_pin(18);   // CLK2

    // Counter outputs.
    pin_out_[0] = pin(10);   // OUT0 -> IRQ0
    pin_out_[1] = pin(13);   // OUT1 -> DMA refresh
    pin_out_[2] = pin(17);   // OUT2 -> speaker

    // Counter gate inputs.
    pin_gate_[0] = connect_pin(11);  // GATE0 (tied to +5V)
    pin_gate_[1] = connect_pin(14);  // GATE1 (tied to +5V)
    pin_gate_[2] = connect_pin(16);  // GATE2 (PPI PB0)

    // Address and control.
    pin_a0_  = pin(19);      // A0
    pin_a1_  = pin(20);      // A1
    pin_cs_  = pin(21);      // ~CS
    pin_rd_  = connect_pin(22);      // ~RD
    pin_wr_  = connect_pin(23);      // ~WR
    pin_vcc_ = connect_pin(24);      // VCC

    // Pin directions for DAG.
    for (int i = 0; i < 3; ++i) { declare_input(pin_clk_[i]); declare_input(pin_gate_[i]); }
    for (int i = 0; i < 3; ++i) declare_output(pin_out_[i]);
    declare_input(pin_a0_); declare_input(pin_a1_); declare_input(pin_cs_);
    declare_input(pin_rd_); declare_input(pin_wr_);

    // Data bus is bidirectional: input during CPU writes, output during CPU reads.
    // The direction lambda is sampled BEFORE evaluate() runs -- it reflects
    // the bus state established by the PREVIOUS cycle (8288 asserts ~IOR/~IOW
    // one cycle before the data is sampled).
    declare_bidir_block(
        {pin_data_[0], pin_data_[1], pin_data_[2], pin_data_[3],
         pin_data_[4], pin_data_[5], pin_data_[6], pin_data_[7]},
        BidirDir::HiZ | BidirDir::Input | BidirDir::Output,
        [this]() -> BidirDir {
            if (pin_cs_.level() != Level::Low) return BidirDir::HiZ;
            if (pin_rd_.level() == Level::Low) return BidirDir::Output;
            if (pin_wr_.level() == Level::Low) return BidirDir::Input;
            return BidirDir::HiZ;
        });
}

// =========================================================================
// One call = one full PIT clock cycle.
// Bidir lambda already set the bus direction before we get here.
// =========================================================================

void IC_8253::on_cycle(Fiber /*caller*/) {
    bool cs_low = pin_cs_.level() == Level::Low;
    bool wr_low = pin_wr_.level() == Level::Low;
    bool rd_low = pin_rd_.level() == Level::Low;

    if (read_pending_ || write_pending_)
    {
        if (read_pending_) {
            handle_read();
            read_pending_ = false;
        }

        if (write_pending_) {
            handle_write();
            write_pending_ = false;
        }
    } else if ((rd_low && !rd_prev_) || (wr_low && !wr_prev_)) {
        // Edge detection: only trigger on falling edge of ~RD/~WR.
        if (rd_low && !rd_prev_) {
            read_pending_ = true;
        }
        if (wr_low && !wr_prev_) {
            write_pending_ = true;
        }
    } else if (data_bus_driven_ && !rd_low) {
        release_data_bus();
    }
    wr_prev_ = wr_low;
    rd_prev_ = rd_low;

    // PIT runs at 1/4 system CLK
    if (pit_timer_ % 4 == 0)
    {
        for (int i = 0; i < 3; ++i)
            channels_[i].gate = pin_gate_[i].level() == Level::High;

        for (int i = 0; i < 3; ++i)
            tick(i);

        // Speaker: sample channel 2 OUT at PIT tick rate (~1.193 MHz),
        // box-average every 25 ticks, push one sample at ~47.7 kHz.
        if (speaker_) {
            bool out = channels_[2].out && speaker_->params().pit_output_enabled;
            spk_accum_ += out ? 1.0f : 0.0f;
            if (++spk_count_ >= SPEAKER_SAMPLE_RATIO) {
                speaker_->push_sample(spk_accum_ / SPEAKER_SAMPLE_RATIO);
                spk_accum_ = 0.0f;
                spk_count_ = 0;
            }
        }
    }

    ++pit_timer_;
}

// =========================================================================
// Bus write
// =========================================================================

void IC_8253::handle_write() {
    uint8_t data = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_data_[i].level() == Level::High)
            data |= (1 << i);
    }

    bool a0 = pin_a0_.level() == Level::High;
    bool a1 = pin_a1_.level() == Level::High;
    int addr = (a1 ? 2 : 0) | (a0 ? 1 : 0);


    if (addr == 3)
        write_control(data);
    else
        write_counter(addr, data);
}

// =========================================================================
// Bus read
// =========================================================================

void IC_8253::handle_read() {
    bool a0 = pin_a0_.level() == Level::High;
    bool a1 = pin_a1_.level() == Level::High;
    int addr = (a1 ? 2 : 0) | (a0 ? 1 : 0);

    if (addr < 3) {
        uint8_t val = read_counter(addr);
        drive_data_bus(val);
    }
}

// =========================================================================
// Control word
// =========================================================================

void IC_8253::write_control(uint8_t value) {
    int ch = (value >> 6) & 3;
    if (ch == 3) return;  // Read-back (8254 only)

    int rw = (value >> 4) & 3;
    if (rw == 0) {
        // Counter latch command.
        if (!channels_[ch].latched) {
            channels_[ch].latch = channels_[ch].count;
            channels_[ch].latched = true;
            channels_[ch].read_msb_next = false;
        }
        return;
    }

    Channel& c = channels_[ch];
    c.mode = (value >> 1) & 7;
    if (c.mode > 5) c.mode &= 3;
    c.bcd = (value & 1) != 0;
    c.rw_mode = rw;
    c.programmed = true;
    c.counting = false;
    c.loaded = false;
    c.null_count = true;
    c.load_lsb_pending = false;
    c.read_msb_next = false;
    c.latched = false;

    switch (c.mode) {
        case 0: c.out = false; break;
        case 1: c.out = true;  break;
        case 2: c.out = true;  break;
        case 3: c.out = true;  break;
        case 4: c.out = true;  break;
        case 5: c.out = true;  break;
    }
    update_out(ch);

}

// =========================================================================
// Counter load
// =========================================================================

void IC_8253::write_counter(int ch, uint8_t value) {
    Channel& c = channels_[ch];
    if (!c.programmed) return;

    switch (c.rw_mode) {
        case 1:
            c.reload = value ? value : 256;
            c.loaded = true;
            c.null_count = true;
            c.counting = true;
            break;
        case 2: {
            uint16_t raw = static_cast<uint16_t>(value) << 8;
            c.reload = raw ? raw : 65536;
            c.loaded = true;
            c.null_count = true;
            c.counting = true;
            break;
        }
        case 3:
            if (!c.load_lsb_pending) {
                c.load_lsb_value = value;
                c.load_lsb_pending = true;
            } else {
                uint16_t raw = (static_cast<uint16_t>(value) << 8) | c.load_lsb_value;
                c.reload = raw ? raw : 65536;
                c.load_lsb_pending = false;
                c.loaded = true;
                c.null_count = true;
                c.counting = true;
            }
            break;
    }
}

// =========================================================================
// Counter read
// =========================================================================

uint8_t IC_8253::read_counter(int ch) {
    Channel& c = channels_[ch];
    uint16_t val = c.latched ? c.latch : c.count;

    uint8_t result = 0;
    switch (c.rw_mode) {
        case 1:
            result = val & 0xFF;
            c.latched = false;
            break;
        case 2:
            result = (val >> 8) & 0xFF;
            c.latched = false;
            break;
        case 3:
            if (!c.read_msb_next) {
                result = val & 0xFF;
                c.read_msb_next = true;
            } else {
                result = (val >> 8) & 0xFF;
                c.read_msb_next = false;
                c.latched = false;
            }
            break;
    }
    return result;
}

// =========================================================================
// Tick -- one PIT clock cycle per channel
// =========================================================================

void IC_8253::tick(int ch) {
    Channel& c = channels_[ch];
    if (!c.programmed || !c.loaded) return;

    // Transfer reload -> count on first tick after load.
    if (c.null_count) {
        c.count = c.reload;
        c.null_count = false;
    }

    if (!c.counting) return;

    switch (c.mode) {
        case 0:
            // Interrupt on terminal count.
            // GATE low suspends counting.
            if (!c.gate) return;
            c.count = decrement(c.count, c.bcd);
            if (c.count == 0) {
                c.out = true;
                update_out(ch);
            }
            break;

        case 1:
            // Hardware retriggerable one-shot.
            // GATE rising edge reloads (handled in gate sampling).
            // Counts regardless of GATE level.
            c.count = decrement(c.count, c.bcd);
            if (c.count == 0) {
                c.out = true;
                c.counting = false;
                update_out(ch);
            }
            break;

        case 2:
            // Rate generator.
            // GATE low suspends counting, forces OUT high.
            if (!c.gate) {
                if (!c.out) { c.out = true; update_out(ch); }
                return;
            }
            c.count = decrement(c.count, c.bcd);
            if (c.count == 1) {
                c.out = false;
                update_out(ch);
            } else if (c.count == 0) {
                c.out = true;
                c.count = c.reload;
                update_out(ch);
            }
            break;

        case 3:
            // Square wave generator.
            // GATE low suspends counting, forces OUT high.
            if (!c.gate) {
                if (!c.out) { c.out = true; update_out(ch); }
                return;
            }
            // Decrements by 2 each tick. Toggle OUT when count expires.
            c.count -= 2;
            if (static_cast<int32_t>(c.count) <= 0) {
                c.out = !c.out;
                c.count = c.reload;
                update_out(ch);
            }
            break;

        case 4:
            // Software triggered strobe.
            // GATE low suspends counting.
            if (!c.gate) return;
            c.count = decrement(c.count, c.bcd);
            if (c.count == 0) {
                c.out = false;
                update_out(ch);
            } else if (!c.out) {
                c.out = true;
                c.counting = false;
                update_out(ch);
            }
            break;

        case 5:
            // Hardware triggered strobe.
            // Counts regardless of GATE level.
            c.count = decrement(c.count, c.bcd);
            if (c.count == 0) {
                c.out = false;
                update_out(ch);
            } else if (!c.out) {
                c.out = true;
                c.counting = false;
                update_out(ch);
            }
            break;
    }
}

// =========================================================================
// Helpers
// =========================================================================

uint32_t IC_8253::decrement(uint32_t val, bool bcd) {
    if (!bcd)
        return val - 1;
    if (val == 0) return 0x9999;
    uint16_t result = 0;
    int borrow = 1;
    for (int i = 0; i < 4; ++i) {
        int digit = (val >> (i * 4)) & 0xF;
        digit -= borrow;
        if (digit < 0) { digit += 10; borrow = 1; }
        else borrow = 0;
        result |= (digit & 0xF) << (i * 4);
    }
    return result;
}

void IC_8253::update_out(int ch) {
    // Channel 1 drives DRQ0 for DMA refresh. Suppress it to avoid
    // refresh cycles complicating the DAG permutation ordering.
    if (ch == 1) return;
    pin_out_[ch].drive(channels_[ch].out ? Level::High : Level::Low);
}

void IC_8253::drive_data_bus(uint8_t value) {
    for (int i = 0; i < 8; ++i)
        pin_data_[i].drive((value & (1 << i)) ? Level::High : Level::Low);
    data_bus_driven_ = true;
}

void IC_8253::release_data_bus() {
    if (data_bus_driven_) {
        for (int i = 0; i < 8; ++i)
            pin_data_[i].release();
        data_bus_driven_ = false;
    }
}

} // namespace bench
