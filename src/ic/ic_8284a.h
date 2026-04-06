#pragma once
#include "core/threaded_component.h"
#include "board/socket.h"
#include <atomic>

namespace bench {

class Scheduler;
class IC_8288;

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
    void set_bus_controller(IC_8288* bc) { bus_ctrl_ = bc; }
    uint64_t clk_cycles() const { return clk_cycles_; }
    const uint64_t& clk_cycles_ref() const { return clk_cycles_; }

    // --- Mock PSU (driven from main thread, acted on by clock thread) ---
    void psu_power_on()  { psu_cmd_ = PsuCmd::PowerOn; }
    void psu_power_off() { psu_cmd_ = PsuCmd::PowerOff; }
    void psu_reset()     { psu_cmd_ = PsuCmd::Reset; }
    void psu_nmi_raise() { psu_nmi_ = true; }
    void psu_nmi_lower() { psu_nmi_ = false; }

    // Give the PSU pin handles to signals it needs to drive.
    // Call once during wiring, before power_on().
    void psu_wire(Pin vcc, Pin gnd, Pin res, Pin nmi,
                  Pin s0, Pin s1, Pin s2, Pin aen) {
        psu_vcc_ = vcc; psu_gnd_ = gnd; psu_res_ = res; psu_nmi_pin_ = nmi;
        psu_s0_ = s0; psu_s1_ = s1; psu_s2_ = s2; psu_aen_ = aen;
    }

    void save(cereal::BinaryOutputArchive& ar) override { serialize(ar); }
    void load(cereal::BinaryInputArchive& ar) override { serialize(ar); }
    template <class Archive> void serialize(Archive& ar) {
        ar(clk_cycles_, reset_hold_, psu_nmi_);
    }

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
    IC_8288* bus_ctrl_ = nullptr;
    uint64_t clk_cycles_ = 0;

    // --- PSU state ---
    enum class PsuCmd : int { None, PowerOn, PowerOff, Reset };
    PsuCmd psu_cmd_ = PsuCmd::None;
    int reset_hold_ = 0;   // cycles remaining in reset pulse
    bool psu_nmi_ = false;
    Pin psu_vcc_, psu_gnd_, psu_res_, psu_nmi_pin_;
    Pin psu_s0_, psu_s1_, psu_s2_, psu_aen_;
};

} // namespace bench
