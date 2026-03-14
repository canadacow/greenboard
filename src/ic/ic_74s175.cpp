#include "ic/ic_74s175.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S175::IC_74S175() : InlineComponent("74S175") { set_description("Quad D Flip-Flop"); }

void IC_74S175::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    pin_clr_ = connect_pin(1);
    pin_clk_ = connect_pin(9);
    pin_vcc_ = connect_pin(16);

    pin_d_[0] = pin(4);
    pin_d_[1] = pin(5);
    pin_d_[2] = pin(12);
    pin_d_[3] = pin(15);

    pin_q_[0] = pin(2);
    pin_q_[1] = pin(7);
    pin_q_[2] = pin(10);
    pin_q_[3] = pin(14);

    pin_nq_[0] = pin(3);
    pin_nq_[1] = pin(6);
    pin_nq_[2] = pin(11);
    pin_nq_[3] = pin(13);

    // Pin directions for wiring visualization.
    declare_input(pin_clr_); declare_input(pin_clk_);
    for (int i = 0; i < 4; ++i) declare_input(pin_d_[i]);
    for (int i = 0; i < 4; ++i) { declare_output(pin_q_[i]); declare_output(pin_nq_[i]); }
}

void IC_74S175::on_signal_change(Fiber /*caller*/, bool rising, bool /*falling*/) {
    if (!rising) return;  // only process on rising half

    // ~CLR: async clear when driven Low.
    Level cur = pin_clr_.level();
    if (cur == Level::Low && clr_prev_ != Level::Low)
        clear_all();
    clr_prev_ = cur;

    // Latch D inputs on rising edge.
    if (pin_clr_.level() != Level::Low)
        on_clk_rising();
}

void IC_74S175::on_clk_rising() {
    // Sample all D inputs and update Q state.
    for (int i = 0; i < 4; ++i) {
        q_[i] = pin_d_[i].level() == Level::High;
    }
    drive_outputs();
}

void IC_74S175::clear_all() {
    for (int i = 0; i < 4; ++i)
        q_[i] = false;
    drive_outputs();
}

void IC_74S175::drive_outputs() {
    for (int i = 0; i < 4; ++i) {
        pin_q_[i].drive(q_[i] ? Level::High : Level::Low);
        pin_nq_[i].drive(q_[i] ? Level::Low : Level::High);
    }
}

} // namespace bench
