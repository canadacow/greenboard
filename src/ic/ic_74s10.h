#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S10 Triple 3-Input NAND Gate (Schottky).
//
// 14-pin DIP. U84 on the 5150.
//
// Pinout:
//   Pin  1: A1       (gate 1 input)
//   Pin  2: B1       (gate 1 input)
//   Pin  3: A2       (gate 2 input)
//   Pin  4: B2       (gate 2 input)
//   Pin  5: C2       (gate 2 input)
//   Pin  6: Y2       (gate 2 output)
//   Pin  7: GND
//   Pin  8: Y3       (gate 3 output)
//   Pin  9: A3       (gate 3 input)
//   Pin 10: B3       (gate 3 input)
//   Pin 11: C3       (gate 3 input)
//   Pin 12: Y1       (gate 1 output)
//   Pin 13: C1       (gate 1 input)
//   Pin 14: VCC
//
// Behavior (per gate):
//   Y = ~(A & B & C)
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74S10 : public CallbackComponent {
public:
    IC_74S10();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change(Fiber caller) override;

private:
    void update_outputs();

    struct Gate {
        Pin a, b, c, y;
    };
    Gate gates_[3];
};

} // namespace bench
