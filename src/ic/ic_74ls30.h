#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74LS30 8-Input NAND Gate (Low-Power Schottky).
//
// 14-pin DIP. U5 on the 5150 (bus grant logic).
//
// Pinout:
//   Pin  1: A       (input)
//   Pin  2: B       (input)
//   Pin  3: C       (input)
//   Pin  4: D       (input)
//   Pin  5: E       (input)
//   Pin  6: F       (input)
//   Pin  7: GND
//   Pin  8: Y       (output = ~(A & B & C & D & E & F & G & H))
//   Pin  9: NC
//   Pin 10: NC
//   Pin 11: G       (input)
//   Pin 12: H       (input)
//   Pin 13: NC
//   Pin 14: VCC
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74LS30 : public CallbackComponent {
public:
    IC_74LS30();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_cycle(Fiber caller) override;

private:
    void update_output();

    Pin inputs_[8];   // A-H: pins 1-6, 11, 12
    Pin output_;      // Y: pin 8
};

} // namespace bench
