#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74LS32 Quad 2-Input OR Gate.
//
// 14-pin DIP. U101 on the 5150.
//
// Pinout (same as 74S08/74S00):
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
//   Y = A | B
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74LS32 : public CallbackComponent {
public:
    IC_74LS32();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change(Fiber caller) override;

private:
    void update_outputs();

    struct Gate {
        Pin a, b, y;
    };
    Gate gates_[4];
};

} // namespace bench
