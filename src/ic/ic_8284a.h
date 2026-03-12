#pragma once
#include "core/threaded_component.h"
#include "board/socket.h"

namespace bench {

// Intel 8284A Clock Generator / Driver.
//
// 18-pin DIP. Generates the master clock for the IBM PC 5150.
//
// Pin functions (from BRD/datasheet):
//   Pin  1: CSYNC    (input, crystal sync -- tied to GND)
//   Pin  2: PCLK     (output, peripheral clock = CLK/2)
//   Pin  3: ~AEN1    (input, ready enable 1)
//   Pin  4: RDY1     (input, ready input 1)
//   Pin  5: READY    (output, synchronized ready)
//   Pin  6: RDY2     (input, ready input 2 -- GND, masked)
//   Pin  7: ~AEN2    (input, ready enable 2 -- +5V, disabled)
//   Pin  8: CLK      (output, system clock = OSC/3, 4.77 MHz)
//   Pin  9: GND
//   Pin 10: RESET    (output, active high)
//   Pin 11: RES      (input, from PWR_GOOD via RC)
//   Pin 12: OSC      (output, oscillator = 14.31818 MHz)
//   Pin 13: F/~C     (input, freq/crystal select -- GND = crystal mode)
//   Pin 14: EFI      (input, external freq -- NC)
//   Pin 15: X2       (crystal pin 2)
//   Pin 16: X1       (crystal pin 1)
//   Pin 17: TANK     (LC tank for crystal)
//   Pin 18: VCC      (+5V)
//
// Thread model:
//   Reactive -- woken each Scheduler tick. on_signal_change() advances
//   the oscillator state machine by one OSC half-period. When VCC is
//   low, does nothing. When VCC drops, releases all outputs.
class IC_8284A : public ThreadedComponent {
public:
    IC_8284A();

    void install(Socket& socket);

protected:
    void on_signal_change() override;
    void on_power_on() override;
    void on_power_off() override;

private:
    // Output pins
    Signal* pin_osc_   = nullptr;   // Pin 12: OSC (14.31818 MHz)
    Signal* pin_clk_   = nullptr;   // Pin  8: CLK (4.77 MHz)
    Signal* pin_pclk_  = nullptr;   // Pin  2: PCLK (2.38 MHz)
    Signal* pin_ready_ = nullptr;   // Pin  5: READY
    Signal* pin_reset_ = nullptr;   // Pin 10: RESET

    // Input pins
    Signal* pin_res_   = nullptr;   // Pin 11: RES (PWR_GOOD)
    Signal* pin_rdy1_  = nullptr;   // Pin  4: RDY1
    Signal* pin_aen1_  = nullptr;   // Pin  3: ~AEN1
    Signal* pin_vcc_   = nullptr;   // Pin 18: VCC

    // Oscillator state machine
    bool running_ = false;          // VCC is high, oscillator active
    int osc_count_ = 0;             // OSC half-period counter (0..5)
    bool clk_state_ = false;
    bool pclk_state_ = false;
    bool osc_state_ = false;
    uint64_t total_ticks_ = 0;
};

} // namespace bench
