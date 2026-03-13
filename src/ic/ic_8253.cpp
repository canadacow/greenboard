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
}

void IC_8253::on_signal_change(bool rising, bool /*falling*/) {
    if (!rising) return;  // compute once per cycle
    // Single-tick model: decrement each tick.
    for (int i = 0; i < 3; ++i) {
        on_clk_falling(i);
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

    // ~RD falling edge: CPU reads from PIT.
    {
        Level cur = pin_rd_.level();
        if (cur == Level::Low && rd_prev_ != Level::Low)
            on_read_falling();
        rd_prev_ = cur;
    }
}

// =========================================================================
// Bus operations
// =========================================================================

void IC_8253::on_write_falling() {
    // Only respond if chip-selected.
    if (pin_cs_.level() != Level::Low) return;

    // Read data bus.
    uint8_t data = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_data_[i].level() == Level::High)
            data |= (1 << i);
    }

    // Decode address.
    bool a0 = pin_a0_.level() == Level::High;
    bool a1 = pin_a1_.level() == Level::High;
    int addr = (a1 ? 2 : 0) | (a0 ? 1 : 0);

    if (addr == 3) {
        write_control(data);
    } else {
        write_counter(addr, data);
    }
}

void IC_8253::on_read_falling() {
    if (pin_cs_.level() != Level::Low) return;

    bool a0 = pin_a0_.level() == Level::High;
    bool a1 = pin_a1_.level() == Level::High;
    int addr = (a1 ? 2 : 0) | (a0 ? 1 : 0);

    if (addr >= 3) return;  // Control word is write-only.

    uint8_t data = read_counter(addr);

    // Drive data bus.
    for (int i = 0; i < 8; ++i) {
        pin_data_[i].drive((data & (1 << i)) ? Level::High : Level::Low);
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

    // Mode-specific OUT initialization.
    switch (c.mode) {
        case 0:
            c.out = false;  // OUT goes low after control word
            break;
        case 1:
            c.out = true;   // OUT is high until triggered
            break;
        case 2:
        case 3:
            c.out = true;   // OUT starts high
            break;
        case 4:
            c.out = true;   // OUT is high, goes low at TC
            break;
        case 5:
            c.out = true;   // OUT is high until triggered
            break;
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
    // Subtract 1 in BCD
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
            // GATE low suspends counting. High resumes.
            break;
        case 1:
            // Rising edge of GATE triggers one-shot.
            if (rising) {
                c.count = c.reload;
                c.null_count = false;
                c.out = false;
                c.counting = true;
                update_out(ch);
            }
            break;
        case 2:
            // Rising edge reloads counter.
            if (rising) {
                c.count = c.reload;
                c.null_count = false;
                c.counting = true;
            }
            // GATE low forces OUT high.
            if (!new_gate) {
                c.out = true;
                update_out(ch);
            }
            break;
        case 3:
            // Rising edge reloads counter.
            if (rising) {
                c.count = c.reload;
                c.null_count = false;
                c.counting = true;
            }
            // GATE low forces OUT high.
            if (!new_gate) {
                c.out = true;
                update_out(ch);
            }
            break;
        case 4:
            // GATE low suspends counting.
            break;
        case 5:
            // Rising edge triggers strobe.
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
            // Mode 0: Interrupt on terminal count.
            // Count down. OUT goes high when count reaches 0.
            c.count = decrement(c.count, c.bcd);
            if (c.count == 0) {
                c.out = true;
                update_out(ch);
            }
            break;
        }

        case 1: {
            // Mode 1: Hardware retriggerable one-shot.
            // Count down after GATE trigger. OUT goes high at TC.
            c.count = decrement(c.count, c.bcd);
            if (c.count == 0) {
                c.out = true;
                c.counting = false;
                update_out(ch);
            }
            break;
        }

        case 2: {
            // Mode 2: Rate generator.
            // OUT is high for N-1 ticks, low for 1 tick, repeat.
            c.count = decrement(c.count, c.bcd);
            if (c.count == 1) {
                c.out = false;
                update_out(ch);
            } else if (c.count == 0) {
                c.out = true;
                c.count = c.reload;  // Auto-reload
                update_out(ch);
            }
            break;
        }

        case 3: {
            // Mode 3: Square wave generator.
            // OUT toggles at half the count period.
            // Decrements by 2 each CLK. When reaching 0, toggle OUT and reload.
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
            // Mode 4: Software triggered strobe.
            // OUT goes low for 1 CLK at terminal count, then high.
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
            // Mode 5: Hardware triggered strobe.
            // Same as mode 4 but triggered by GATE.
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
