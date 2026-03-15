#include "ic/ic_74s04.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S04::IC_74S04() : CallbackComponent("74S04") { set_description("Hex Inverter"); }

void IC_74S04::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    // Gate 1: pin 1 -> pin 2
    gates_[0].a = connect_pin(1);
    gates_[0].y = pin(2);

    // Gate 2: pin 3 -> pin 4
    gates_[1].a = connect_pin(3);
    gates_[1].y = pin(4);

    // Gate 3: pin 5 -> pin 6
    gates_[2].a = connect_pin(5);
    gates_[2].y = pin(6);

    // Gate 4: pin 9 -> pin 8
    gates_[3].a = connect_pin(9);
    gates_[3].y = pin(8);

    // Gate 5: pin 11 -> pin 10
    gates_[4].a = connect_pin(11);
    gates_[4].y = pin(10);

    // Gate 6: pin 13 -> pin 12
    gates_[5].a = connect_pin(13);
    gates_[5].y = pin(12);

    // VCC
    Signal* vcc = socket.pin_signal(14);
    if (vcc) vcc->connect(this);

    // Pin directions
    for (auto& g : gates_) {
        declare_input(g.a);
        declare_output(g.y);
    }
}

void IC_74S04::on_power_on() {
    update_outputs();
}

void IC_74S04::on_power_off() {
    for (auto& g : gates_)
        g.y.release();
}

void IC_74S04::on_signal_change(Fiber /*caller*/) {
    update_outputs();
}

void IC_74S04::update_outputs() {
    for (auto& g : gates_) {
        bool high = g.a.level() == Level::High;
        g.y.drive(high ? Level::Low : Level::High);
    }
}

} // namespace bench
