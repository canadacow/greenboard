#include "ic/ic_74s74.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S74::IC_74S74(bool async_clk) : CallbackComponent("74S74"), async_clk_(async_clk) { set_description("Dual D Flip-Flop"); }

void IC_74S74::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    // FF1
    ff_[0].clr = connect_pin(1);
    ff_[0].d   = connect_pin(2);
    ff_[0].clk = connect_pin(3);
    ff_[0].pre = connect_pin(4);
    ff_[0].q   = pin(5);
    ff_[0].nq  = pin(6);

    // FF2
    ff_[1].clr = connect_pin(13);
    ff_[1].d   = connect_pin(12);
    ff_[1].clk = connect_pin(11);
    ff_[1].pre = connect_pin(10);
    ff_[1].q   = pin(9);
    ff_[1].nq  = pin(8);

    // VCC
    Signal* vcc = socket.pin_signal(14);
    if (vcc) vcc->connect(this);

    // D pins are async: sampled on CLK edge only, not combinational.
    // CLK is sync by default but can be made async to break registered
    // feedback cycles (e.g. U82 where CLK comes from downstream of Q).
    for (auto& f : ff_) {
        declare_input(f.clr);
        declare_async_input(f.d);
        if (async_clk_)
            declare_async_input(f.clk);
        else
            declare_input(f.clk);
        declare_input(f.pre);
        declare_output(f.q);
        declare_output(f.nq);
    }
}

void IC_74S74::on_power_on() {
    for (int i = 0; i < 2; ++i)
        update_ff(i);
}

void IC_74S74::on_power_off() {
    for (auto& f : ff_) {
        f.q.release();
        f.nq.release();
    }
}

void IC_74S74::on_signal_change(Fiber /*caller*/) {
    for (int i = 0; i < 2; ++i)
        update_ff(i);
}

void IC_74S74::update_ff(int i) {
    auto& f = ff_[i];
    bool clr = f.clr.level() == Level::Low;
    bool pre = f.pre.level() == Level::Low;

    bool old_q = f.q_state;

    if (clr && pre) {
        // Both asserted: Q=H, ~Q=H (indeterminate)
        f.q_state = true;
        f.q.drive(Level::High);
        f.nq.drive(Level::High);
        f.clk_prev = f.clk.level();
    } else if (clr) {
        f.q_state = false;
        drive_ff(i);
        f.clk_prev = f.clk.level();
    } else if (pre) {
        f.q_state = true;
        drive_ff(i);
        f.clk_prev = f.clk.level();
    } else {
        // Latch D when CLK is High.
        // The real 74S74 is edge-triggered, but in this architecture CLK
        // is never driven by the 8284A (each evaluate() = one implicit tick).
        // Signals derived from CLK (e.g. nclk88) are held constant, so edge
        // detection never fires.  Transparent-latch on CLK High matches the
        // 74S175 approach used elsewhere.
        Level clk_now = f.clk.level();
        if (clk_now == Level::High) {
            f.q_state = f.d.level() == Level::High;
        }
        f.clk_prev = clk_now;
        drive_ff(i);
    }

    spdlog::trace("[{}] FF{}: D={} CLK={} ~CLR={} ~PRE={} -> Q={} (was {})",
                  name(), i + 1,
                  int(f.d.level()), int(f.clk.level()),
                  int(f.clr.level()), int(f.pre.level()),
                  f.q_state ? 1 : 0, old_q ? 1 : 0);
    if (f.q_state != old_q) {
        spdlog::debug("[{}] FF{}: Q CHANGED {} -> {}", name(), i + 1, old_q ? 1 : 0, f.q_state ? 1 : 0);
    }
}

void IC_74S74::drive_ff(int i) {
    auto& f = ff_[i];
    f.q.drive(f.q_state ? Level::High : Level::Low);
    f.nq.drive(f.q_state ? Level::Low : Level::High);
}

} // namespace bench
