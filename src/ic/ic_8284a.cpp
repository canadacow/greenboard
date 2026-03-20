#include "ic/ic_8284a.h"
#include "core/scheduler.h"
#include "host_platform/fiber.h"
#include "host_platform/thread_util.h"
#include <spdlog/spdlog.h>
#include <thread>

namespace bench {

IC_8284A::IC_8284A() : ThreadedComponent("8284A") { set_description("Clock Generator"); }

void IC_8284A::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    pin_osc_   = pin(12);  // OSC output
    pin_clk_   = pin(8);   // CLK output
    pin_pclk_  = pin(2);   // PCLK output
    pin_ready_ = pin(5);   // READY output
    pin_reset_ = pin(10);  // RESET output
    pin_res_   = pin(11);  // RES input (PWR_GOOD)
    pin_rdy1_  = pin(4);   // RDY1 input
    pin_aen1_  = pin(3);   // ~AEN1 input
    pin_vcc_   = pin(18);  // VCC

    // Pin directions for wiring visualization.
    declare_input(pin_res_); declare_input(pin_rdy1_); declare_input(pin_aen1_);
    declare_output(pin_osc_); declare_output(pin_clk_); declare_output(pin_pclk_);
    declare_output(pin_ready_); declare_output(pin_reset_);
}

void IC_8284A::run(std::stop_token stop) {
    // Hot thread -- maximize scheduling priority and pin to P-cores.
    thread_set_time_critical();
    thread_pin_to_pcores();

    // Convert this thread to a fiber so we can switch to component fibers.
    Fiber self = fiber_convert_thread();

    // Wait for PSU power-on command (main thread calls psu_power_on()).
    while (!stop.stop_requested()) {
        if (psu_cmd_ == PsuCmd::PowerOn) {
            psu_cmd_ = PsuCmd::None;
#ifdef BENCH_PIN_VALIDATION
            SignalPool::set_clock_thread();
#endif
            // Power on all components before driving VCC (like seating ICs).
            scheduler_->power_on_all();
            psu_gnd_.drive(Level::Low);
            psu_s0_.drive(Level::High);
            psu_s1_.drive(Level::High);
            psu_s2_.drive(Level::High);
            psu_aen_.drive(Level::High);
            psu_vcc_.drive(Level::High);
            psu_res_.drive(Level::High);
            break;
        }
    }
    if (stop.stop_requested()) {
        fiber_revert_thread(self);
        return;
    }

    spdlog::debug("[8284A] VCC is High, oscillator spinning");

    // RESET: the mock PSU drives RES straight to High (no RC ramp),
    // so RESET (inverted RES) is Low for the entire run. Drive once.
    pin_reset_.drive(Level::Low);

    // READY: synchronized from RDY1/~AEN1 each CLK cycle (see spin loop).
    // Start High (no wait states until DMA asserts).
    pin_ready_.drive(Level::High);

    // Clock state.
    Level pclk_level = Level::Low;
    bool nmi_state = false;

    // --- Spin loop: one iteration = one full CLK cycle (rise + fall) ---
    // The real 8284A divides a 14.318 MHz crystal by 3 to produce CLK,
    // toggling OSC each tick and only changing CLK every 3rd toggle.
    // That means 4 out of 6 ticks per CLK cycle do nothing but increment
    // a counter -- and nothing on the board observes OSC directly.
    // So we skip the divide-by-3 and just strobe CLK high/low.
    while (!stop.stop_requested()) {
        // PSU commands (checked each cycle, relaxed is fine).
        if (psu_cmd_ == PsuCmd::PowerOff) {
            psu_res_.drive(Level::Low);
            psu_vcc_.drive(Level::HiZ);
            spdlog::debug("[8284A] PSU power-off, oscillator stopped after {} CLK cycles", clk_cycles_);
            break;
        }

        // NMI: only drive on change.
        if (psu_nmi_ != nmi_state) {
            nmi_state = psu_nmi_;
            psu_nmi_pin_.drive(psu_nmi_ ? Level::High : Level::Low);
        }

        // PCLK toggles each CLK cycle (CLK / 2).
        pclk_level = Level(int8_t(-int8_t(pclk_level)));
        pin_pclk_.drive(pclk_level);

        spdlog::trace("[CLK] cycle={} READY={} pclk_level={} (~AEN1={} RDY1={})", clk_cycles_, 
            int(pin_ready_.level()), int(pin_pclk_.level()),
            int(pin_aen1_.level()), int(pin_rdy1_.level())
        );

        scheduler_->evaluate(self);

        // READY: synchronize from RDY1 (~DMA_WAIT) gated by ~AEN1 (~RDY/WAIT).
        // When ~AEN1 is Low (active), READY follows RDY1.
        // When ~AEN1 is High (disabled), READY is High (no wait).
        // On the 5150, ~AEN1 = ~RDY/WAIT from U82, RDY1 = ~DMA_WAIT from U98.
        // During DMA: ~DMA_WAIT goes Low -> READY Low -> CPU stalls in Tw.
        {
            Level aen1 = pin_aen1_.level();
            Level rdy1 = pin_rdy1_.level();
            Level ready = (aen1 == Level::Low && rdy1 == Level::Low)
                        ? Level::Low : Level::High;
            pin_ready_.drive(ready);
        }

        ++clk_cycles_;
    }

    // Power down: release all outputs, then power off all components.
    pin_osc_.release();
    pin_clk_.release();
    pin_pclk_.release();
    pin_ready_.release();
    pin_reset_.release();
    scheduler_->evaluate();
    scheduler_->power_off_all();

    spdlog::debug("[8284A] oscillator stopped after {} CLK cycles", clk_cycles_);

    // Revert back to a plain thread before returning.
    fiber_revert_thread(self);
}

} // namespace bench
