#pragma once
#include "core/threaded_component.h"
#include "board/socket.h"

namespace bench {

class Scheduler;

// Intel 8284A Clock Generator / Driver.
//
// 18-pin DIP. Generates the master clock for the IBM PC 5150.
//
// Pin functions (from BRD/datasheet):
//   Pin  2: PCLK     (output, peripheral clock = CLK/2)
//   Pin  3: ~AEN1    (input, ready enable 1)
//   Pin  4: RDY1     (input, ready input 1)
//   Pin  5: READY    (output, synchronized ready)
//   Pin  8: CLK      (output, system clock = OSC/3, 4.77 MHz)
//   Pin 10: RESET    (output, active high)
//   Pin 11: RES      (input, from PWR_GOOD via RC)
//   Pin 12: OSC      (output, oscillator = 14.31818 MHz)
//   Pin 18: VCC      (+5V)
//
// Thread model:
//   Active IC -- its thread IS the crystal oscillator. Spin loop
//   toggles OSC, divides to CLK/PCLK. On each CLK edge: calls
//   scheduler->evaluate() to commit signals, eval inline ICs,
//   and run all fiber components cooperatively.
class IC_8284A : public ThreadedComponent {
public:
    IC_8284A();

    void install(Socket& socket);

    void set_scheduler(Scheduler* s) { scheduler_ = s; }
    uint64_t clk_cycles() const { return clk_cycles_; }

protected:
    void run(std::stop_token stop) override;

private:
    // Output pins
    Pin pin_osc_;
    Pin pin_clk_;
    Pin pin_pclk_;
    Pin pin_ready_;
    Pin pin_reset_;

    // Input pins
    Pin pin_res_;
    Pin pin_rdy1_;
    Pin pin_aen1_;
    Pin pin_vcc_;

    Scheduler* scheduler_ = nullptr;
    uint64_t clk_cycles_ = 0;
};

} // namespace bench
