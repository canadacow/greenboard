#pragma once
#include "core/inline_component.h"
#include "board/socket.h"

namespace bench {

// 74S138 3-to-8 Line Decoder/Demultiplexer.
//
// 16-pin DIP. U46, U47, U48, U65, U66 on the 5150 (address decode).
//
// Standard pinout:
//   Pin  1: A        (select input)
//   Pin  2: B        (select input)
//   Pin  3: C        (select input)
//   Pin  4: ~G2A     (enable, active low)
//   Pin  5: ~G2B     (enable, active low)
//   Pin  6: G1       (enable, active high)
//   Pin  7: ~Y7      (output, active low)
//   Pin  8: GND
//   Pin  9: ~Y6
//   Pin 10: ~Y5
//   Pin 11: ~Y4
//   Pin 12: ~Y3
//   Pin 13: ~Y2
//   Pin 14: ~Y1
//   Pin 15: ~Y0
//   Pin 16: VCC
//
// Behavior:
//   When G1=High AND ~G2A=Low AND ~G2B=Low:
//     The output selected by CBA (0-7) goes Low, all others High.
//   Otherwise:
//     All outputs High (inactive).
//
// Threading: InlineComponent -- combinational, no thread.
class IC_74S138 : public InlineComponent {
public:
    IC_74S138();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change() override;

private:
    void update_outputs();

    // Select inputs
    Pin a_;     // Pin 1: A (LSB)
    Pin b_;     // Pin 2: B
    Pin c_;     // Pin 3: C (MSB)

    // Enable inputs
    Pin g2a_;   // Pin 4: ~G2A (active low)
    Pin g2b_;   // Pin 5: ~G2B (active low)
    Pin g1_;    // Pin 6: G1 (active high)

    // Outputs (~Y0 through ~Y7, active low)
    Pin y_[8];
};

} // namespace bench
