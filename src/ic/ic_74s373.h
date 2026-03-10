#pragma once
#include "core/component.h"
#include "board/socket.h"

namespace bench {

// 74S373 Octal Transparent Latch.
//
// 20-pin DIP. U7, U9, U10 on the 5150 (address latches).
//
// Standard pinout:
//   Pin  1: ~OE (output enable, active low)
//   Pin  2: Q0    Pin  3: D0
//   Pin  4: D1    Pin  5: Q1
//   Pin  6: Q2    Pin  7: D2
//   Pin  8: D3    Pin  9: Q3
//   Pin 10: GND
//   Pin 11: LE (latch enable: High=transparent, Low=latched)
//   Pin 12: Q4    Pin 13: D4
//   Pin 14: D5    Pin 15: Q5
//   Pin 16: Q6    Pin 17: D6
//   Pin 18: D7    Pin 19: Q7
//   Pin 20: VCC
//
// Behavior:
//   LE High + ~OE Low  -> transparent: Q tracks D, outputs driven
//   LE Low  + ~OE Low  -> latched: Q holds last value, outputs driven
//   ~OE High           -> outputs tri-stated regardless of LE
//
// Threading: Reactive IC. Default run() -- blocks on mailbox.
class IC_74S373 : public Component {
public:
    IC_74S373();

    void install(Socket& socket);

protected:
    void on_signal_change(Signal& signal, Level old_level, Level new_level) override;

private:
    void update_outputs();

    // D/Q pin pairs (8 channels): D[i] -> latch -> Q[i]
    Signal* pin_d_[8] = {};  // D0=pin3,D1=pin4,D2=pin7,D3=pin8,D4=pin13,D5=pin14,D6=pin17,D7=pin18
    Signal* pin_q_[8] = {};  // Q0=pin2,Q1=pin5,Q2=pin6,Q3=pin9,Q4=pin12,Q5=pin15,Q6=pin16,Q7=pin19

    Signal* pin_le_ = nullptr;   // Pin 11: LE
    Signal* pin_oe_ = nullptr;   // Pin  1: ~OE
    Signal* pin_vcc_ = nullptr;  // Pin 20: VCC

    Level latch_[8] = {};  // Latched values
};

} // namespace bench
