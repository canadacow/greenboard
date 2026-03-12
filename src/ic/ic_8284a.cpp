#include "ic/ic_8284a.h"
#include "core/scheduler.h"
#include "host_platform/fiber.h"
#include <spdlog/spdlog.h>
#include <thread>

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
}

void IC_8284A::run(std::stop_token stop) {
    // Convert this thread to a fiber so we can switch to component fibers.
    Fiber self = fiber_convert_thread();

    // Wait for VCC to go High (poll -- nobody wakes us before the loop starts).
    while (!stop.stop_requested()) {
        if (pin_vcc_ && pin_vcc_->level() == Level::High) break;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    if (stop.stop_requested()) {
        fiber_revert_thread(self);
        return;
    }

    spdlog::debug("[8284A] VCC is High, oscillator spinning");

    // Assert RESET on power-up (RES starts low from RC delay).
    if (pin_reset_) pin_reset_->drive(Level::High);

    // Clock state.
    bool pclk_state = false;
    uint64_t clk_ticks = 0;

    // --- Spin loop: one iteration = one full CLK cycle (rise + fall) ---
    // The real 8284A divides a 14.318 MHz crystal by 3 to produce CLK,
    // toggling OSC each tick and only changing CLK every 3rd toggle.
    // That means 4 out of 6 ticks per CLK cycle do nothing but increment
    // a counter -- and nothing on the board observes OSC directly.
    // So we skip the divide-by-3 and just strobe CLK high/low.
    while (!stop.stop_requested()) {
        // Check VCC each cycle.
        if (pin_vcc_ && pin_vcc_->level() != Level::High) {
            spdlog::debug("[8284A] VCC dropped, oscillator stopped after {} CLK cycles", clk_ticks);
            break;
        }

        // --- CLK rising edge ---
        if (pin_clk_) pin_clk_->drive(Level::High);

        // PCLK toggles on CLK rising edge (CLK / 2).
        ++clk_cycles_;
        pclk_state = !pclk_state;
        if (pin_pclk_) pin_pclk_->drive(pclk_state ? Level::High : Level::Low);

        scheduler_->evaluate(self);

        // --- CLK falling edge ---
        if (pin_clk_) pin_clk_->drive(Level::Low);

        // READY and RESET are synchronized to CLK falling edge.
        bool ready = true;
        bool aen1 = pin_aen1_ && pin_aen1_->level() == Level::Low;
        if (aen1)
            ready = pin_rdy1_ && pin_rdy1_->level() == Level::High;
        if (pin_ready_) pin_ready_->drive(ready ? Level::High : Level::Low);

        // RESET: inverted and synchronized RES input.
        bool res = pin_res_ && pin_res_->level() == Level::High;
        if (pin_reset_) pin_reset_->drive(res ? Level::Low : Level::High);

        scheduler_->evaluate(self);

        ++clk_ticks;
    }

    // Power down: release all outputs.
    if (pin_osc_)   pin_osc_->release();
    if (pin_clk_)   pin_clk_->release();
    if (pin_pclk_)  pin_pclk_->release();
    if (pin_ready_) pin_ready_->release();
    if (pin_reset_) pin_reset_->release();
    scheduler_->evaluate_no_wake();

    spdlog::debug("[8284A] oscillator stopped after {} CLK cycles", clk_ticks);

    // Revert back to a plain thread before returning.
    fiber_revert_thread(self);
}

} // namespace bench
