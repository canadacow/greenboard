#include "ic/ic_8284a.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8284A::IC_8284A() : Component("8284A") {}

void IC_8284A::install(Socket& socket) {
    // Bind to socket pin signals (already wired by BRD parser).
    pin_osc_   = socket.pin_signal(12);  // OSC output
    pin_clk_   = socket.pin_signal(8);   // CLK output
    pin_pclk_  = socket.pin_signal(2);   // PCLK output
    pin_ready_ = socket.pin_signal(5);   // READY output
    pin_reset_ = socket.pin_signal(10);  // RESET output
    pin_res_   = socket.pin_signal(11);  // RES input (PWR_GOOD)
    pin_rdy1_  = socket.pin_signal(4);   // RDY1 input
    pin_aen1_  = socket.pin_signal(3);   // ~AEN1 input
    pin_vcc_   = socket.pin_signal(18);  // VCC

    // Subscribe to input signals so we get mailbox events.
    if (pin_res_)  pin_res_->connect(this);
    if (pin_rdy1_) pin_rdy1_->connect(this);
    if (pin_aen1_) pin_aen1_->connect(this);
    if (pin_vcc_)  pin_vcc_->connect(this);

    spdlog::debug("[8284A] installed into socket {}", socket.ref());
}

void IC_8284A::run(std::stop_token stop) {
    spdlog::debug("[8284A] thread started, waiting for VCC");

    // Wait for VCC to go High.
    while (!stop.stop_requested()) {
        if (pin_vcc_ && pin_vcc_->level() == Level::High)
            break;
        wait_mailbox(stop);
        Signal::ack();
    }

    if (stop.stop_requested()) return;
    spdlog::debug("[8284A] VCC is High, oscillator spinning");

    // Assert RESET on power-up (RES starts low from RC delay).
    if (pin_reset_) pin_reset_->drive(Level::High);

    // --- Oscillator spin loop ---
    // OSC toggles every tick.
    // CLK = OSC / 3 (33% duty: high for 1 OSC cycle, low for 2).
    // PCLK = CLK / 2 (50% duty).
    int osc_count = 0;     // counts OSC half-periods (0..5 for one CLK cycle)
    bool clk_state = false;
    bool pclk_state = false;
    bool osc_state = false;
    uint64_t total_ticks = 0;

    while (!stop.stop_requested()) {
        // Check VCC is still on.
        if (pin_vcc_ && pin_vcc_->level() != Level::High) {
            spdlog::debug("[8284A] VCC dropped, oscillator stopped after {} ticks", total_ticks);
            break;
        }

        // Toggle OSC.
        osc_state = !osc_state;
        if (pin_osc_) pin_osc_->drive(osc_state ? Level::High : Level::Low);

        // Divide by 3 for CLK.
        // 6 OSC half-periods = 1 CLK cycle.
        // CLK high for 2 OSC half-periods, low for 4 (33% duty cycle).
        osc_count = (osc_count + 1) % 6;
        bool new_clk = (osc_count < 2);
        if (new_clk != clk_state) {
            clk_state = new_clk;
            if (pin_clk_) pin_clk_->drive(clk_state ? Level::High : Level::Low);

            // On CLK falling edge: update READY and RESET (synchronized to CLK).
            if (!clk_state) {
                // READY: RDY1 is valid when ~AEN1 is active (low).
                bool ready = true;
                bool aen1 = pin_aen1_ && pin_aen1_->level() == Level::Low;
                if (aen1)
                    ready = pin_rdy1_ && pin_rdy1_->level() == Level::High;
                if (pin_ready_) pin_ready_->drive(ready ? Level::High : Level::Low);

                // RESET: inverted and synchronized RES input.
                // RES high (PWR_GOOD) = no reset. RES low = assert reset.
                bool res = pin_res_ && pin_res_->level() == Level::High;
                if (pin_reset_) pin_reset_->drive(res ? Level::Low : Level::High);
            }

            // Divide CLK by 2 for PCLK (on CLK rising edge).
            if (clk_state) {
                pclk_state = !pclk_state;
                if (pin_pclk_) pin_pclk_->drive(pclk_state ? Level::High : Level::Low);
            }
        }

        ++total_ticks;
    }

    // Power down: release all outputs.
    if (pin_osc_)   pin_osc_->release();
    if (pin_clk_)   pin_clk_->release();
    if (pin_pclk_)  pin_pclk_->release();
    if (pin_ready_) pin_ready_->release();
    if (pin_reset_) pin_reset_->release();

    spdlog::debug("[8284A] oscillator stopped");
}

} // namespace bench
