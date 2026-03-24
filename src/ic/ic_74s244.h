#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S244 Octal Tri-State Buffer (Schottky, non-inverting).
//
// 20-pin DIP. U15, U16, U17, U23 on the 5150.
//
// Two independent groups of 4 buffers, each with its own ~G (enable).
// Interleaved pinout: group 1 inputs on top-left, outputs on bottom-right,
// group 2 inputs on bottom-left, outputs on top-right.
//
// Pinout:
//   Pin  1: ~1G     (group 1 enable, active low)
//   Pin  2: 1A1     (group 1 input)
//   Pin  3: 2Y4     (group 2 output)
//   Pin  4: 1A2     (group 1 input)
//   Pin  5: 2Y3     (group 2 output)
//   Pin  6: 1A3     (group 1 input)
//   Pin  7: 2Y2     (group 2 output)
//   Pin  8: 1A4     (group 1 input)
//   Pin  9: 2Y1     (group 2 output)
//   Pin 10: GND
//   Pin 11: 2A1     (group 2 input)
//   Pin 12: 1Y4     (group 1 output)
//   Pin 13: 2A2     (group 2 input)
//   Pin 14: 1Y3     (group 1 output)
//   Pin 15: 2A3     (group 2 input)
//   Pin 16: 1Y2     (group 1 output)
//   Pin 17: 2A4     (group 2 input)
//   Pin 18: 1Y1     (group 1 output)
//   Pin 19: ~2G     (group 2 enable, active low)
//   Pin 20: VCC
//
// Behavior:  Y = A when ~G Low,  Y = HiZ when ~G High.
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74S244 : public CallbackComponent {
public:
    IC_74S244();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_cycle(Fiber caller) override;

private:
    void update_outputs();

    struct Buffer {
        Pin a, y;    // input, output
    };
    Buffer grp1_[4];  // group 1: pins 2->18, 4->16, 6->14, 8->12
    Buffer grp2_[4];  // group 2: pins 11->9, 13->7, 15->5, 17->3
    Pin pin_g1_;      // ~1G (pin 1)
    Pin pin_g2_;      // ~2G (pin 19)
};

} // namespace bench
