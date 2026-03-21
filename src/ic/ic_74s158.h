#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S158 Quad 2-to-1 Multiplexer with Inverted Outputs (Schottky).
//
// 16-pin DIP. U62 and U79 on the 5150 (DRAM address multiplexer).
//
// Pinout:
//   Pin  1: SELECT   (common select for all 4 muxes)
//   Pin  2: I0a      (mux A, input 0)
//   Pin  3: I1a      (mux A, input 1)
//   Pin  4: Ya       (mux A, inverted output)
//   Pin  5: I0b      (mux B, input 0)
//   Pin  6: I1b      (mux B, input 1)
//   Pin  7: Yb       (mux B, inverted output)
//   Pin  8: GND
//   Pin  9: Yc       (mux C, inverted output)
//   Pin 10: I1c      (mux C, input 1)
//   Pin 11: I0c      (mux C, input 0)
//   Pin 12: Yd       (mux D, inverted output)
//   Pin 13: I1d      (mux D, input 1)
//   Pin 14: I0d      (mux D, input 0)
//   Pin 15: ~STROBE  (active-low enable; active = outputs follow mux)
//   Pin 16: VCC
//
// Behavior (per channel, active when ~STROBE is Low):
//   Y = ~(SELECT ? I1 : I0)
//   When ~STROBE is High, all Y outputs are High.
//
// On the 5150: SELECT = ADDR_SEL, ~STROBE = GND (always enabled).
//   Low: MA = ~(row address A0-A7)
//   High: MA = ~(col address A8-A15)
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74S158 : public CallbackComponent {
public:
    IC_74S158();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change(Fiber caller) override;

private:
    void update_outputs();

    Pin pin_select_;   // Pin 1
    Pin pin_strobe_;   // Pin 15

    struct Mux {
        Pin i0, i1, y;
    };
    Mux muxes_[4];
    IC_74S158* partner_ = nullptr;

public:
    // Link the high-nibble mux so this (low) mux can log full bytes.
    void set_partner(IC_74S158* hi) { partner_ = hi; }
    const Mux* muxes() const { return muxes_; }
};

} // namespace bench
