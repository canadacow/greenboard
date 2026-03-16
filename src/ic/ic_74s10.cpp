#include "ic/ic_74s10.h"

namespace bench {

IC_74S10::IC_74S10() : CallbackComponent("74S10") { set_description("Triple 3-In NAND"); }

void IC_74S10::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    // Gate 1: pins 1,2,13 -> 12
    gates_[0].a = connect_pin(1);
    gates_[0].b = connect_pin(2);
    gates_[0].c = connect_pin(13);
    gates_[0].y = pin(12);

    // Gate 2: pins 3,4,5 -> 6
    gates_[1].a = connect_pin(3);
    gates_[1].b = connect_pin(4);
    gates_[1].c = connect_pin(5);
    gates_[1].y = pin(6);

    // Gate 3: pins 9,10,11 -> 8
    gates_[2].a = connect_pin(9);
    gates_[2].b = connect_pin(10);
    gates_[2].c = connect_pin(11);
    gates_[2].y = pin(8);

    // VCC
    Signal* vcc = socket.pin_signal(14);
    if (vcc) vcc->connect(this);

    // Pin directions
    for (auto& g : gates_) {
        declare_input(g.a);
        declare_input(g.b);
        declare_input(g.c);
        declare_output(g.y);
    }
}

void IC_74S10::on_power_on() {
    update_outputs();
}

void IC_74S10::on_power_off() {
    for (auto& g : gates_)
        g.y.release();
}

void IC_74S10::on_signal_change(Fiber /*caller*/) {
    update_outputs();
}

void IC_74S10::update_outputs() {
    for (auto& g : gates_) {
        bool all = g.a.level() == Level::High &&
                   g.b.level() == Level::High &&
                   g.c.level() == Level::High;
        g.y.drive(all ? Level::Low : Level::High);
    }
}

} // namespace bench
