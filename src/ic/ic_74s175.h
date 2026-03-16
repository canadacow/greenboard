#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S175 Quad D Flip-Flop (Schottky).
//
// 16-pin DIP. Four positive-edge-triggered D flip-flops with
// complementary outputs and asynchronous master clear.
//
// Pin functions:
//   Pin  1: ~CLR  (input, master clear -- active low, async)
//   Pin  2: 1Q    (output)
//   Pin  3: ~1Q   (output, complement)
//   Pin  4: 1D    (input)
//   Pin  5: 2D    (input)
//   Pin  6: ~2Q   (output, complement)
//   Pin  7: 2Q    (output)
//   Pin  8: GND
//   Pin  9: CLK   (input, clock -- rising edge triggered)
//   Pin 10: 3Q    (output)
//   Pin 11: ~3Q   (output, complement)
//   Pin 12: 3D    (input)
//   Pin 13: ~4Q   (output, complement)
//   Pin 14: 4Q    (output)
//   Pin 15: 4D    (input)
//   Pin 16: VCC
//
// U26 on the 5150:
//   FF0+FF1: 2-stage keyboard data synchronizer (KBD_DATA -> 1D -> 1Q=2D -> ~2Q)
//   FF2: PCLK / 2 divider (~3Q fed back to 3D, 3Q = 1.193 MHz PIT clock)
//   FF3: unused
//
// Threading: Inline IC. Executes synchronously in the driving thread.
// CLK rising edge latches D inputs, ~CLR async clears all outputs.
class IC_74S175 : public CallbackComponent {
public:
    IC_74S175();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change(Fiber caller) override;

private:
    void on_clk_rising();
    void clear_all();
    void drive_outputs();

    // Pin handles
    Pin pin_clr_;    // Pin 1: ~CLR
    Pin pin_clk_;    // Pin 9: CLK
    Pin pin_vcc_;    // Pin 16: VCC

    // D inputs: 1D=pin4, 2D=pin5, 3D=pin12, 4D=pin15
    Pin pin_d_[4];

    // Q outputs: 1Q=pin2, 2Q=pin7, 3Q=pin10, 4Q=pin14
    Pin pin_q_[4];

    // ~Q outputs: ~1Q=pin3, ~2Q=pin6, ~3Q=pin11, ~4Q=pin13
    Pin pin_nq_[4];

    // Internal state
    bool q_[4] = {};
    Level clr_prev_ = Level::HiZ;
};

} // namespace bench
