#include "ic/ic_74ls02.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74LS02::IC_74LS02() : CallbackComponent("74LS02") { set_description("Quad 2-In NOR"); }

void IC_74LS02::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    // 74LS02 NOR pinout: outputs are on pins 1,4,8,11 (same positions as 74S00/08)
    // Gate 1: pins 2,3 -> 1
    gates_[0].a = connect_pin(2);
    gates_[0].b = connect_pin(3);
    gates_[0].y = pin(1);

    // Gate 2: pins 5,6 -> 4
    gates_[1].a = connect_pin(5);
    gates_[1].b = connect_pin(6);
    gates_[1].y = pin(4);

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

void IC_74LS02::on_power_on() {
    update_outputs();
}

void IC_74LS02::on_power_off() {
    for (auto& g : gates_)
        g.y.release();
}

void IC_74LS02::on_signal_change(Fiber /*caller*/) {
    update_outputs();
}

void IC_74LS02::update_outputs() {
    for (int i = 0; i < 4; ++i) {
        auto& g = gates_[i];
        bool any = g.a.level() == Level::High || g.b.level() == Level::High;
        Level out = any ? Level::Low : Level::High;
        spdlog::trace("[{}] gate{}: A={} B={} -> Y={}", name(), i+1,
                      int(g.a.level()), int(g.b.level()), int(out));
        g.y.drive(out);
    }
}

} // namespace bench
