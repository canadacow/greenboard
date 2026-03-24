#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S04 Hex Inverter (Schottky).
//
// 14-pin DIP. U51, U83, U99 on the 5150.
//
// Pinout:
//   Pin  1: A1       (gate 1 input)
//   Pin  2: Y1       (gate 1 output)
//   Pin  3: A2       (gate 2 input)
//   Pin  4: Y2       (gate 2 output)
//   Pin  5: A3       (gate 3 input)
//   Pin  6: Y3       (gate 3 output)
//   Pin  7: GND
//   Pin  8: Y4       (gate 4 output)
//   Pin  9: A4       (gate 4 input)
//   Pin 10: Y5       (gate 5 output)
//   Pin 11: A5       (gate 5 input)
//   Pin 12: Y6       (gate 6 output)
//   Pin 13: A6       (gate 6 input)
//   Pin 14: VCC
//
// Behavior (per gate):
//   Y = ~A   (TTL: HiZ treated as High input -> Low output)
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74S04 : public CallbackComponent {
public:
    IC_74S04() : CallbackComponent("74S04") { set_description("Hex Inverter"); }

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

        constexpr int pins[6][2] = {
            {1, 2}, {3, 4}, {5, 6}, {9, 8}, {11, 10}, {13, 12}
        };

        for (int i = 0; i < 6; ++i) {
            gates_[i].a = connect_pin(pins[i][0]);
            gates_[i].y = pin(pins[i][1]);
            declare_input(gates_[i].a);
            declare_output(gates_[i].y);
        }

        // VCC
        Signal* vcc = socket.pin_signal(14);
        if (vcc) vcc->connect(this);
    }

protected:
    void on_power_on() override { }

    void on_power_off() override { }

    void on_cycle(Fiber /*caller*/) override { 
        gates_[0].y.drive((Level)(-(int)gates_[0].a.level() | (int)Level::High));
        gates_[1].y.drive((Level)(-(int)gates_[1].a.level() | (int)Level::High));
        gates_[2].y.drive((Level)(-(int)gates_[2].a.level() | (int)Level::High));
        gates_[3].y.drive((Level)(-(int)gates_[3].a.level() | (int)Level::High));
        gates_[4].y.drive((Level)(-(int)gates_[4].a.level() | (int)Level::High));
        gates_[5].y.drive((Level)(-(int)gates_[5].a.level() | (int)Level::High));
     }

private:

    struct Gate { Pin a, y; };
    Gate gates_[6];
};

} // namespace bench
