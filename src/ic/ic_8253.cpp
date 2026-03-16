#include "ic/ic_8253.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8253::IC_8253() : CallbackComponent("8253") { set_description("PIT"); }

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
// Main signal change handler -- called by scheduler in wave order
// =========================================================================

void IC_8253::on_signal_change(Fiber /*caller*/) {
    // CLK falling edge: decrement each channel on its own CLK transition.
    for (int i = 0; i < 3; ++i) {
        Level cur = pin_clk_[i].level();
        if (cur == Level::Low && clk_prev_[i] == Level::High)
            on_clk_falling(i);
        clk_prev_[i] = cur;
    }

    // GATE level changes.
    for (int i = 0; i < 3; ++i) {
        Level cur = pin_gate_[i].level();
        if (cur != gate_prev_[i])
            on_gate_change(i, cur == Level::High);
        gate_prev_[i] = cur;
    }

    // ~WR falling edge: CPU writes to PIT.
    {
        Level cur = pin_wr_.level();
        if (cur == Level::Low && wr_prev_ != Level::Low)
            on_write_falling();
        wr_prev_ = cur;
    }

    // ~RD: drive data bus while active, release when inactive.
    // Level-sensitive (not just edge): re-drive every cycle that ~CS+~RD
    // are both low, so the data bus stays valid across the full read window.
    {
        Level cur = pin_rd_.level();
        bool cs_low = pin_cs_.level() == Level::Low;

        if (cur == Level::Low && cs_low) {
            // Active read -- drive (or re-drive) data bus.
            if (!data_bus_driven_) {
                // First cycle: detect address and latch the read value.
                bool a0 = pin_a0_.level() == Level::High;
                bool a1 = pin_a1_.level() == Level::High;
                int addr = (a1 ? 2 : 0) | (a0 ? 1 : 0);
                if (addr < 3) {
                    read_byte_ = read_counter(addr);
                    drive_data_bus(read_byte_);
                }
            } else {
                // Subsequent cycles: re-drive same value (bus hold).
                drive_data_bus(read_byte_);
            }
        } else if (data_bus_driven_) {
            // ~RD or ~CS went inactive: release bus.
            release_data_bus();
        }

        rd_prev_ = cur;
    }
}

// =========================================================================
// Bus operations
// =========================================================================

void IC_8253::on_write_falling() {
    if (pin_cs_.level() != Level::Low) return;

    uint8_t data = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_data_[i].level() == Level::High)
            data |= (1 << i);
    }

    bool a0 = pin_a0_.level() == Level::High;
    bool a1 = pin_a1_.level() == Level::High;
    int addr = (a1 ? 2 : 0) | (a0 ? 1 : 0);

    if (addr == 3) {
        write_control(data);
    } else {
        write_counter(addr, data);
    }
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

// =========================================================================
// Control word
// =========================================================================

void IC_8253::write_control(uint8_t value) {
    int ch = (value >> 6) & 3;
    if (ch == 3) {
        // Read-back command (8254 only, not on 8253). Ignore.
        return;
    }

    int rw = (value >> 4) & 3;
    if (rw == 0) {
        // Counter latch command: snapshot current count for reading.
        // Per datasheet: if already latched, the command is ignored.
        if (!channels_[ch].latched) {
            channels_[ch].latch = channels_[ch].count;
            channels_[ch].latched = true;
            channels_[ch].read_msb_next = false;
        }
        return;
    }

    Channel& c = channels_[ch];
    c.mode = (value >> 1) & 7;
    if (c.mode > 5) c.mode &= 3;  // Modes 6,7 map to 2,3.
    c.bcd = (value & 1) != 0;
    c.rw_mode = rw;
    c.programmed = true;
    c.counting = false;
    c.loaded = false;
    c.null_count = true;
    c.load_lsb_pending = false;
    c.read_msb_next = false;
    c.latched = false;

    // Mode-specific OUT initialization per datasheet.
    switch (c.mode) {
        case 0: c.out = false; break;  // OUT goes low immediately
        case 1: c.out = true;  break;  // OUT high until gate trigger
        case 2: c.out = true;  break;  // OUT starts high
        case 3: c.out = true;  break;  // OUT starts high
        case 4: c.out = true;  break;  // OUT high, low at TC
        case 5: c.out = true;  break;  // OUT high until gate trigger
    }
    update_out(ch);

    spdlog::debug("[8253] ch{} programmed: mode={}, rw={}, bcd={}",
                  ch, c.mode, c.rw_mode, c.bcd);
}

// =========================================================================
// Counter read/write
// =========================================================================

void IC_8253::write_counter(int ch, uint8_t value) {
    Channel& c = channels_[ch];
    if (!c.programmed) return;

    switch (c.rw_mode) {
        case 1:  // LSB only
            c.reload = value;
            c.loaded = true;
            c.null_count = true;
            c.counting = true;
            break;
        case 2:  // MSB only
            c.reload = static_cast<uint16_t>(value) << 8;
            c.loaded = true;
            c.null_count = true;
            c.counting = true;
            break;
        case 3:  // LSB then MSB
            if (!c.load_lsb_pending) {
                c.load_lsb_value = value;
                c.load_lsb_pending = true;
            } else {
                c.reload = (static_cast<uint16_t>(value) << 8) | c.load_lsb_value;
                c.load_lsb_pending = false;
                c.loaded = true;
                c.null_count = true;
                c.counting = true;
            }
            break;
    }
}

uint8_t IC_8253::read_counter(int ch) {
    Channel& c = channels_[ch];
    uint16_t val = c.latched ? c.latch : c.count;

    uint8_t result = 0;
    switch (c.rw_mode) {
        case 1:  // LSB only
            result = val & 0xFF;
            if (c.latched) c.latched = false;
            break;
        case 2:  // MSB only
            result = (val >> 8) & 0xFF;
            if (c.latched) c.latched = false;
            break;
        case 3:  // LSB then MSB
            if (!c.read_msb_next) {
                result = val & 0xFF;
                c.read_msb_next = true;
            } else {
                result = (val >> 8) & 0xFF;
                c.read_msb_next = false;
                if (c.latched) c.latched = false;
            }
            break;
    }
    return result;
}

// =========================================================================
// Counter decrement helper
// =========================================================================

uint16_t IC_8253::decrement(uint16_t val, bool bcd) {
    if (!bcd) {
        return val - 1;  // Wraps 0 -> 0xFFFF naturally.
    }
    // BCD: 0000 -> 9999
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

// =========================================================================
// GATE change
// =========================================================================

void IC_8253::on_gate_change(int ch, bool new_gate) {
    Channel& c = channels_[ch];
    bool old_gate = c.gate;
    c.gate = new_gate;

    if (!c.programmed || !c.loaded) return;

    bool rising = !old_gate && new_gate;

    switch (c.mode) {
        case 0:
            // GATE low suspends counting, high resumes.
            break;
        case 1:
            if (rising) {
                c.count = c.reload;
                c.null_count = false;
                c.out = false;
                c.counting = true;
                update_out(ch);
            }
            break;
        case 2:
            if (rising) {
                c.count = c.reload;
                c.null_count = false;
                c.counting = true;
            }
            if (!new_gate) {
                c.out = true;
                update_out(ch);
            }
            break;
        case 3:
            if (rising) {
                c.count = c.reload;
                c.null_count = false;
                c.counting = true;
            }
            if (!new_gate) {
                c.out = true;
                update_out(ch);
            }
            break;
        case 4:
            // GATE low suspends counting.
            break;
        case 5:
            if (rising) {
                c.count = c.reload;
                c.null_count = false;
                c.counting = true;
            }
            break;
    }
}

// =========================================================================
// CLK falling edge -- the main counter tick
// =========================================================================

void IC_8253::on_clk_falling(int ch) {
    Channel& c = channels_[ch];
    if (!c.programmed || !c.loaded) return;

    // Transfer reload value to counting element on first tick after load.
    if (c.null_count) {
        c.count = c.reload;
        c.null_count = false;
    }

    if (!c.counting) return;

    // GATE must be high for counting in modes 0, 2, 3, 4.
    if (!c.gate && (c.mode == 0 || c.mode == 2 || c.mode == 3 || c.mode == 4))
        return;

    switch (c.mode) {
        case 0: {
            c.count = decrement(c.count, c.bcd);
            if (c.count == 0) {
                c.out = true;
                update_out(ch);
            }
            break;
        }

        case 1: {
            c.count = decrement(c.count, c.bcd);
            if (c.count == 0) {
                c.out = true;
                c.counting = false;
                update_out(ch);
            }
            break;
        }

        case 2: {
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
        }

        case 3: {
            if (c.count <= 2) {
                c.out = !c.out;
                c.count = c.reload;
                update_out(ch);
            } else {
                c.count -= 2;
            }
            break;
        }

        case 4: {
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

        case 5: {
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
}

// =========================================================================
// Drive the OUT pin to match internal state
// =========================================================================

void IC_8253::update_out(int ch) {
    pin_out_[ch].drive(channels_[ch].out ? Level::High : Level::Low);
}

} // namespace bench
