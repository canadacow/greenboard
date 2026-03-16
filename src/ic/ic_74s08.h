#pragma once
#include "core/callback_component.h"
#include "board/socket.h"
#include <algorithm>

namespace bench {

// 74S08 Quad 2-Input AND Gate (Schottky).
//
// 14-pin DIP. U49/U97 on the 5150.
//
// Pinout:
//   Pin  1: A1       (gate 1 input)
//   Pin  2: B1       (gate 1 input)
//   Pin  3: Y1       (gate 1 output)
//   Pin  4: A2       (gate 2 input)
//   Pin  5: B2       (gate 2 input)
//   Pin  6: Y2       (gate 2 output)
//   Pin  7: GND
//   Pin  8: Y3       (gate 3 output)
//   Pin  9: A3       (gate 3 input)
//   Pin 10: B3       (gate 3 input)
//   Pin 11: Y4       (gate 4 output)
//   Pin 12: A4       (gate 4 input)
//   Pin 13: B4       (gate 4 input)
//   Pin 14: VCC
//
// Behavior (per gate):
//   Y = A & B
//
// Template parameter MASK: bit 0 = gate 1, ... bit 3 = gate 4.
// Only gates with their bit set are evaluated and driven.
//
// Threading: CallbackComponent -- combinational, no fiber.
template<uint8_t MASK = 0x0F>
class IC_74S08 : public CallbackComponent {
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
        gates_[0].a = connect_pin(1);
        gates_[0].b = connect_pin(2);
        gates_[0].y = pin(3);

        // Gate 2: pins 4,5 -> 6
        gates_[1].a = connect_pin(4);
        gates_[1].b = connect_pin(5);
        gates_[1].y = pin(6);

        // Gate 3: pins 9,10 -> 8
        gates_[2].a = connect_pin(9);
        gates_[2].b = connect_pin(10);
        gates_[2].y = pin(8);

        // Gate 4: pins 12,13 -> 11
        gates_[3].a = connect_pin(12);
        gates_[3].b = connect_pin(13);
        gates_[3].y = pin(11);

        for (auto& g : gates_) {
            declare_input(g.a);
            declare_input(g.b);
            declare_output(g.y);
        }

        // Require all 4 output slots contiguous (masked-out gates use dummy signals).
        {
            int y0 = gates_[0].y.idx;
            for (int i = 1; i < 4; ++i) {
                if (gates_[i].y.idx != y0 + i) {
                    spdlog::critical("[{}] output pins not contiguous: gate {} idx {} != {} + {}",
                                     name(), i, gates_[i].y.idx, y0, i);
                    std::_Exit(1);
                }
            }
            out_block_.base = y0;
        }

        Signal* vcc = socket.pin_signal(14);
        if (vcc) vcc->connect(this);
    }

protected:
    void on_power_on() override { update_outputs(); }

    void on_power_off() override {
        for (auto& g : gates_) g.y.release();
    }

    void on_signal_change(Fiber /*caller*/) override { update_outputs(); }

private:
    void update_outputs() {
        // AND: Y = min(A, B). High(1) only when both High(1).
        // Outputs are contiguous -- bulk write via PinBlock<4>.
        Level results[4];
        results[0] = std::min(gates_[0].a.level(), gates_[0].b.level());
        results[1] = std::min(gates_[1].a.level(), gates_[1].b.level());
        results[2] = std::min(gates_[2].a.level(), gates_[2].b.level());
        results[3] = std::min(gates_[3].a.level(), gates_[3].b.level());
        out_block_.drive(results);
    }

    struct Gate {
        Pin a, b, y;
    };
    Gate gates_[4];
    PinBlock<4> out_block_;  // used when all 4 outputs are contiguous
};

} // namespace bench
