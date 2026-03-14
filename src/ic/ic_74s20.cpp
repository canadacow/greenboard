#include "ic/ic_74s20.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S20::IC_74S20() : CallbackComponent("74S20") { set_description("Dual 4-In NAND"); }

void IC_74S20::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    // Gate 1
    a1_ = connect_pin(1);
    b1_ = connect_pin(2);
    c1_ = connect_pin(4);   // Pin 3 is NC
    d1_ = connect_pin(5);
    y1_ = pin(6);

    // Gate 2
    a2_ = connect_pin(9);
    b2_ = connect_pin(10);
    c2_ = connect_pin(12);  // Pin 11 is NC
    d2_ = connect_pin(13);
    y2_ = pin(8);

    // VCC
    Signal* vcc = socket.pin_signal(14);
    if (vcc) vcc->connect(this);

    // Pin directions for wiring visualization.
    declare_input(a1_); declare_input(b1_); declare_input(c1_); declare_input(d1_);
    declare_output(y1_);
    declare_input(a2_); declare_input(b2_); declare_input(c2_); declare_input(d2_);
    declare_output(y2_);
}

void IC_74S20::on_power_on() {
    update_outputs();
}

void IC_74S20::on_power_off() {
    y1_.release();
    y2_.release();
}

void IC_74S20::on_signal_change(Fiber /*caller*/) {
    update_outputs();
}

void IC_74S20::update_outputs() {
    // Gate 1: Y1 = ~(A1 & B1 & C1 & D1)
    bool all1 =
        a1_.level() == Level::High &&
        b1_.level() == Level::High &&
        c1_.level() == Level::High &&
        d1_.level() == Level::High;
    y1_.drive(all1 ? Level::Low : Level::High);

    // Gate 2: Y2 = ~(A2 & B2 & C2 & D2)
    bool all2 =
        a2_.level() == Level::High &&
        b2_.level() == Level::High &&
        c2_.level() == Level::High &&
        d2_.level() == Level::High;
    y2_.drive(all2 ? Level::Low : Level::High);
}

} // namespace bench
