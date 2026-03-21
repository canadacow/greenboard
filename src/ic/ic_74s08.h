#pragma once
#include "core/callback_component.h"
#include "board/socket.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace bench {

// 74S08 Quad 2-Input AND Gate (Schottky).
//
// 14-pin DIP. U49/U97 on the 5150.
//
// Pinout:
//   Pin  1: A1       Pin  8: Y3
//   Pin  2: B1       Pin  9: A3
//   Pin  3: Y1       Pin 10: B3
//   Pin  4: A2       Pin 11: Y4
//   Pin  5: B2       Pin 12: A4
//   Pin  6: Y2       Pin 13: B4
//   Pin  7: GND      Pin 14: VCC
//
// Template parameters:
//   MODES: 2 bits per gate (LSB = gate 0), encoding per-gate behavior:
//     00 = dead      -- output Low, no input reads
//     01 = pass A    -- output = A, one read
//     10 = pass B    -- output = B, one read
//     11 = full AND  -- output = min(A, B), two reads
//   SHARED_B: when true, all B inputs are the same signal.
//     Read b_[0] once and reuse for all gates.
//
// All 4 output pins must be contiguous in SignalPool (asserted at install).
//
// Threading: CallbackComponent -- combinational, no fiber.
template<uint8_t MODES = 0xFF, bool SHARED_B = false>
class IC_74S08 : public CallbackComponent {
    static constexpr uint8_t M0 = MODES & 3;
    static constexpr uint8_t M1 = (MODES >> 2) & 3;
    static constexpr uint8_t M2 = (MODES >> 4) & 3;
    static constexpr uint8_t M3 = (MODES >> 6) & 3;

public:
    IC_74S08() : CallbackComponent("74S08") { set_description("Quad 2-In AND"); }

    void install(Socket& socket) {
        auto connect_pin = [&](int p) -> Pin {
            Signal* s = socket.pin_signal(p);
            if (s) s->connect(this);
            return s ? s->pin() : Pin{};
        };
        auto pin = [&](int p) -> Pin {
            Signal* s = socket.pin_signal(p);
            return s ? s->pin() : Pin{};
        };

        // Gate 1: pins 1,2 -> 3
        a_[0] = connect_pin(1);
        b_[0] = connect_pin(2);
        y_[0] = pin(3);

        // Gate 2: pins 4,5 -> 6
        a_[1] = connect_pin(4);
        b_[1] = connect_pin(5);
        y_[1] = pin(6);

        // Gate 3: pins 9,10 -> 8
        a_[2] = connect_pin(9);
        b_[2] = connect_pin(10);
        y_[2] = pin(8);

        // Gate 4: pins 12,13 -> 11
        a_[3] = connect_pin(12);
        b_[3] = connect_pin(13);
        y_[3] = pin(11);

        for (int i = 0; i < 4; ++i) {
            declare_input(a_[i]);
            declare_input(b_[i]);
            declare_output(y_[i]);
        }

        // Require all 4 output slots contiguous.
        {
            int y0 = y_[0].idx;
            for (int i = 1; i < 4; ++i) {
                if (y_[i].idx != y0 + i) {
                    spdlog::critical("[{}] output pins not contiguous: gate {} idx {} != {} + {}",
                                     name(), i, y_[i].idx, y0, i);
                    std::_Exit(1);
                }
            }
            out_.base = y0;
        }

        Signal* vcc = socket.pin_signal(14);
        if (vcc) vcc->connect(this);
    }

protected:
    void on_power_on() override { update_outputs(); }

    void on_power_off() override { out_.release(); }

    void on_signal_change(Fiber /*caller*/) override { update_outputs(); }

private:
    template<uint8_t MODE>
    static Level gate_eval(Level a, Level b) {
        if constexpr (MODE == 0) return Level::Low;
        else if constexpr (MODE == 1) return a;
        else if constexpr (MODE == 2) return b;
        else return std::min(a, b);
    }

    void update_outputs() {
        Level r[4];
        if constexpr (SHARED_B) {
            Level b = b_[0].level();
            r[0] = gate_eval<M0>(a_[0].level(), b);
            r[1] = gate_eval<M1>(a_[1].level(), b);
            r[2] = gate_eval<M2>(a_[2].level(), b);
            r[3] = gate_eval<M3>(a_[3].level(), b);
        } else {
            r[0] = gate_eval<M0>(a_[0].level(), b_[0].level());
            r[1] = gate_eval<M1>(a_[1].level(), b_[1].level());
            r[2] = gate_eval<M2>(a_[2].level(), b_[2].level());
            r[3] = gate_eval<M3>(a_[3].level(), b_[3].level());
        }
        out_.drive(r);
        spdlog::trace("[{}] gate1: A={} B={} -> Y={}  gate2: A={} B={} -> Y={}  "
                      "gate3: A={} B={} -> Y={}  gate4: A={} B={} -> Y={}",
                      name(),
                      int(a_[0].level()), int(b_[0].level()), int(r[0]),
                      int(a_[1].level()), int(b_[1].level()), int(r[1]),
                      int(a_[2].level()), int(b_[2].level()), int(r[2]),
                      int(a_[3].level()), int(b_[3].level()), int(r[3]));
    }

    Pin a_[4], b_[4], y_[4];
    PinBlock<4> out_;
};

} // namespace bench
