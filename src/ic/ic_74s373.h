#pragma once
#include "core/callback_component.h"
#include "board/socket.h"
#include <cstring>

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
// Threading: InlineComponent -- combinational, no thread.
class IC_74S373 : public CallbackComponent {
public:
    IC_74S373();

    /// Mark D inputs as async (cross-cycle) for DAG ordering.
    /// Use for instances where LE is pulsed briefly (e.g. ADSTB on U18)
    /// so the D->Q path is effectively cross-cycle, not combinational.
    void set_async_inputs() { async_d_ = true; }
    Level oe_level() const { return oe_.level(); }

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_signal_change(Fiber caller) override;

private:
    void update_outputs();

    PinBlock<8> d_;   // D inputs (contiguous)
    PinBlock<8> q_;   // Q outputs (contiguous)
    Pin le_;          // LE (read)
    Pin oe_;          // ~OE (read)

    Level latch_[8] = {};  // Latched values
    Level le_prev_ = Level::HiZ;
    bool oe_active_ = false;   // true when ~OE is Low (outputs driven)
    bool async_d_ = false;
};

} // namespace bench
