#include "ic/ic_8284a.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8284A::IC_8284A() : ThreadedComponent("8284A") {}

void IC_8284A::install(Socket& socket) {
    pin_osc_   = socket.pin_signal(12);  // OSC output
    pin_clk_   = socket.pin_signal(8);   // CLK output
    pin_pclk_  = socket.pin_signal(2);   // PCLK output
    pin_ready_ = socket.pin_signal(5);   // READY output
    pin_reset_ = socket.pin_signal(10);  // RESET output
    pin_res_   = socket.pin_signal(11);  // RES input (PWR_GOOD)
    pin_rdy1_  = socket.pin_signal(4);   // RDY1 input
    pin_aen1_  = socket.pin_signal(3);   // ~AEN1 input
    pin_vcc_   = socket.pin_signal(18);  // VCC

    spdlog::debug("[8284A] installed into socket {}", socket.ref());
}

void IC_8284A::on_power_on() {
    running_ = false;
    osc_count_ = 0;
    clk_state_ = false;
    pclk_state_ = false;
    osc_state_ = false;
    total_ticks_ = 0;
}

void IC_8284A::on_power_off() {
    if (running_) {
        if (pin_osc_)   pin_osc_->release();
        if (pin_clk_)   pin_clk_->release();
        if (pin_pclk_)  pin_pclk_->release();
        if (pin_ready_) pin_ready_->release();
        if (pin_reset_) pin_reset_->release();
        spdlog::debug("[8284A] oscillator stopped after {} ticks", total_ticks_);
    }
    running_ = false;
}

void IC_8284A::on_signal_change() {
    bool vcc_high = pin_vcc_ && pin_vcc_->level() == Level::High;

    // Not yet running -- wait for VCC.
    if (!running_) {
        if (!vcc_high) return;
        running_ = true;
        // Assert RESET on power-up (RES starts low from RC delay).
        if (pin_reset_) pin_reset_->drive(Level::High);
        spdlog::debug("[8284A] VCC is High, oscillator spinning");
        return;  // First real tick on next wake.
    }

    // VCC dropped -- stop.
    if (!vcc_high) {
        spdlog::debug("[8284A] VCC dropped, oscillator stopped after {} ticks", total_ticks_);
        if (pin_osc_)   pin_osc_->release();
        if (pin_clk_)   pin_clk_->release();
        if (pin_pclk_)  pin_pclk_->release();
        if (pin_ready_) pin_ready_->release();
        if (pin_reset_) pin_reset_->release();
        running_ = false;
        return;
    }

    // --- One OSC half-period tick ---
    osc_state_ = !osc_state_;
    if (pin_osc_) pin_osc_->drive(osc_state_ ? Level::High : Level::Low);

    // Divide by 3 for CLK.
    // 6 OSC half-periods = 1 CLK cycle.
    // CLK high for 2 OSC half-periods, low for 4 (33% duty cycle).
    osc_count_ = (osc_count_ + 1) % 6;
    bool new_clk = (osc_count_ < 2);
    if (new_clk != clk_state_) {
        clk_state_ = new_clk;
        if (pin_clk_) pin_clk_->drive(clk_state_ ? Level::High : Level::Low);

        // On CLK falling edge: update READY and RESET (synchronized to CLK).
        if (!clk_state_) {
            bool ready = true;
            bool aen1 = pin_aen1_ && pin_aen1_->level() == Level::Low;
            if (aen1)
                ready = pin_rdy1_ && pin_rdy1_->level() == Level::High;
            if (pin_ready_) pin_ready_->drive(ready ? Level::High : Level::Low);

            // RESET: inverted and synchronized RES input.
            bool res = pin_res_ && pin_res_->level() == Level::High;
            if (pin_reset_) pin_reset_->drive(res ? Level::Low : Level::High);
        }

        // Divide CLK by 2 for PCLK (on CLK rising edge).
        if (clk_state_) {
            pclk_state_ = !pclk_state_;
            if (pin_pclk_) pin_pclk_->drive(pclk_state_ ? Level::High : Level::Low);
        }
    }

    ++total_ticks_;
}

} // namespace bench
