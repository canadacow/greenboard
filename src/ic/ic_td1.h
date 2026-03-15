#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// TD1: DRAM timing delay line (PE-21712).
//
// 14-pin package on the 5150 motherboard.
//
// Pinout (active pins only):
//   Pin  1: RAS       (input)
//   Pin  7: GND
//   Pin  8: N-000258  (output, delayed RAS)
//   Pin 10: ADDR_SEL  (output, delayed RAS)
//   Pin 12: N-000253  (output, delayed RAS)
//   Pin 14: VCC
//
// Behavior:
//   On the real board, TD1 delays RAS by ~100ns to three tapped outputs.
//   In the full-cycle model (~210ns/CLK), this is a one-cycle delay:
//   input is captured, outputs are driven with the previously captured
//   value. Subscribes to CLK for re-evaluation each cycle.
//
// Threading: CallbackComponent -- one state variable, no fiber.
class IC_TD1 : public CallbackComponent {
public:
    IC_TD1();

    void install(Socket& socket);

    // Subscribe to CLK so TD1 re-evaluates each cycle to propagate
    // the captured value even when the input hasn't changed again.
    void connect_clk(Signal& clk);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change(Fiber caller) override;

private:
    Pin pin_in_;        // Pin 1: RAS
    Pin pin_out_[3];    // Pin 8, 10, 12
    Level pending_ = Level::HiZ;
};

} // namespace bench
