#pragma once
#include "core/inline_component.h"
#include "board/socket.h"

namespace bench {

// 74S20 Dual 4-Input NAND Gate.
//
// 14-pin DIP. U64 on the 5150 (ROM address decode).
//
// Pinout:
//   Pin  1: A1       (gate 1 input)
//   Pin  2: B1       (gate 1 input)
//   Pin  3: NC
//   Pin  4: C1       (gate 1 input)
//   Pin  5: D1       (gate 1 input)
//   Pin  6: Y1       (gate 1 output)
//   Pin  7: GND
//   Pin  8: Y2       (gate 2 output)
//   Pin  9: A2       (gate 2 input)
//   Pin 10: B2       (gate 2 input)
//   Pin 11: NC
//   Pin 12: C2       (gate 2 input)
//   Pin 13: D2       (gate 2 input)
//   Pin 14: VCC
//
// Behavior (per gate):
//   Y = ~(A & B & C & D)
//   Output is Low only when all four inputs are High.
//
// Threading: InlineComponent -- combinational, no thread.
class IC_74S20 : public InlineComponent {
public:
    IC_74S20();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change(Fiber caller) override;

private:
    void update_outputs();

    // Gate 1: pins 1,2,4,5 -> pin 6
    Pin a1_, b1_, c1_, d1_, y1_;

    // Gate 2: pins 9,10,12,13 -> pin 8
    Pin a2_, b2_, c2_, d2_, y2_;
};

} // namespace bench
