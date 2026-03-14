#include "ic/ic_74s138.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S138::IC_74S138() : InlineComponent("74S138") { set_description("3-to-8 Decoder"); }

void IC_74S138::install(Socket& socket) {
    // Select inputs
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    a_   = connect_pin(1);
    b_   = connect_pin(2);
    c_   = connect_pin(3);
    g2a_ = connect_pin(4);
    g2b_ = connect_pin(5);
    g1_  = connect_pin(6);

    // Outputs: ~Y0=pin15, ~Y1=pin14, ..., ~Y7=pin7
    static constexpr int y_pins[] = {15, 14, 13, 12, 11, 10, 9, 7};
    for (int i = 0; i < 8; ++i)
        y_[i] = pin(y_pins[i]);

    // VCC
    Signal* vcc = socket.pin_signal(16);
    if (vcc) vcc->connect(this);

    // Pin directions for wiring visualization.
    declare_input(a_); declare_input(b_); declare_input(c_);
    declare_input(g2a_); declare_input(g2b_); declare_input(g1_);
    for (int i = 0; i < 8; ++i) declare_output(y_[i]);
}

void IC_74S138::on_power_on() {
    for (int i = 0; i < 8; ++i)
        y_[i].drive(Level::High);
}

void IC_74S138::on_power_off() {
    for (int i = 0; i < 8; ++i)
        y_[i].release();
}

void IC_74S138::on_signal_change(Fiber caller, bool /*rising*/, bool /*falling*/) {
    update_outputs();
}

void IC_74S138::update_outputs() {
    bool enabled =
        g1_.level()  == Level::High &&
        g2a_.level() == Level::Low &&
        g2b_.level() == Level::Low;

    if (enabled) {
        int sel = 0;
        if (a_.level() == Level::High) sel |= 1;
        if (b_.level() == Level::High) sel |= 2;
        if (c_.level() == Level::High) sel |= 4;

        for (int i = 0; i < 8; ++i)
            y_[i].drive(i == sel ? Level::Low : Level::High);
    } else {
        for (int i = 0; i < 8; ++i)
            y_[i].drive(Level::High);
    }
}

} // namespace bench
