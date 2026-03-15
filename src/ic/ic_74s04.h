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
//   Y = ~A
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74S04 : public CallbackComponent {
public:
    IC_74S04();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change(Fiber caller) override;

private:
    void update_outputs();

    struct Gate {
        Pin a, y;
    };
    Gate gates_[6];
};

} // namespace bench
