#include "ic/ic_74s08.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S08::IC_74S08() : CallbackComponent("74S08") { set_description("Quad 2-In AND"); }

void IC_74S08::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    // Gate 1: pins 1,2 -> 3
    gates_[0].a = connect_pin(1);
    gates_[0].b = connect_pin(2);
    gates_[0].y = pin(3);

    // Gate 2: pins 4,5 -> 6
    gates_[1].a = connect_pin(4);
    gates_[1].b = connect_pin(5);
    gates_[1].y = pin(6);

    // Gate 3: pins 9,10 -> 8
    gates_[2].a = connect_pin(9);
    gates_[2].b = connect_pin(10);
    gates_[2].y = pin(8);

    // Gate 4: pins 12,13 -> 11
    gates_[3].a = connect_pin(12);
    gates_[3].b = connect_pin(13);
    gates_[3].y = pin(11);

    // VCC
    Signal* vcc = socket.pin_signal(14);
    if (vcc) vcc->connect(this);

    // Pin directions
    for (auto& g : gates_) {
        declare_input(g.a);
        declare_input(g.b);
        declare_output(g.y);
    }
}

void IC_74S08::on_power_on() {
    update_outputs();
}

void IC_74S08::on_power_off() {
    for (auto& g : gates_)
        g.y.release();
}

void IC_74S08::on_signal_change(Fiber /*caller*/) {
    update_outputs();
}

void IC_74S08::update_outputs() {
    for (int i = 0; i < 4; ++i) {
        auto& g = gates_[i];
        bool both = g.a.level() == Level::High && g.b.level() == Level::High;
        g.y.drive(both ? Level::High : Level::Low);
    }
}

} // namespace bench
