#pragma once
#include "core/component.h"
#include "board/socket.h"

namespace bench {

// 74S245 Octal Bus Transceiver.
//
// 20-pin DIP. U8 on the 5150 (data bus transceiver).
//
// Standard pinout:
//   Pin  1: ~G (gate/enable, active low)
//   Pin  2: A1    Pin 18: B1
//   Pin  3: A2    Pin 17: B2
//   Pin  4: A3    Pin 16: B3
//   Pin  5: A4    Pin 15: B4
//   Pin  6: A5    Pin 14: B5
//   Pin  7: A6    Pin 13: B6
//   Pin  8: A7    Pin 12: B7
//   Pin  9: A8    Pin 11: B8
//   Pin 10: GND
//   Pin 19: DIR (direction: High=A->B, Low=B->A)
//   Pin 20: VCC
//
// 5150 wiring (U8):
//   A side = CPU local bus (AD0-AD7)
//   B side = system data bus (D0-D7)
//   ~G = ~DEN from 8288
//   DIR = DT/~R from 8288 (via gating logic)
//
// Threading: Reactive IC. Default run() -- blocks on mailbox.
class IC_74S245 : public Component {
public:
    IC_74S245();

    void install(Socket& socket);

protected:
    void on_signal_change(Signal& signal, Level old_level, Level new_level) override;

private:
    void update_outputs();
    void release_all();

    Signal* pin_a_[8] = {};   // A1=pin2 .. A8=pin9
    Signal* pin_b_[8] = {};   // B1=pin18 .. B8=pin11
    Signal* pin_g_   = nullptr;  // Pin  1: ~G (enable)
    Signal* pin_dir_ = nullptr;  // Pin 19: DIR
    Signal* pin_vcc_ = nullptr;  // Pin 20: VCC
};

} // namespace bench
