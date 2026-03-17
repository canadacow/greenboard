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

    // 74LS02 NOR pinout (NOT same as 74S00/08!):
    //   Gates 1,2: Y first  (Y,A,B) -> pins 1,2,3 and 4,5,6
    //   Gates 3,4: Y last   (A,B,Y) -> pins 8,9,10 and 11,12,13
    // Gate 1: pins 2,3 -> 1
    gates_[0].a = connect_pin(2);
    gates_[0].b = connect_pin(3);
    gates_[0].y = pin(1);

    // Gate 2: pins 5,6 -> 4
    gates_[1].a = connect_pin(5);
    gates_[1].b = connect_pin(6);
    gates_[1].y = pin(4);

    // Gate 3: pins 8,9 -> 10
    gates_[2].a = connect_pin(8);
    gates_[2].b = connect_pin(9);
    gates_[2].y = pin(10);

    // Gate 4: pins 11,12 -> 13
    gates_[3].a = connect_pin(11);
    gates_[3].b = connect_pin(12);
    gates_[3].y = pin(13);

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
    // Two passes: gate outputs may feed other gates within the same IC
    // (e.g. U27: gate 4 output -> gate 1 input). First pass settles
    // inter-gate dependencies, second pass produces final values.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 4; ++i) {
            auto& g = gates_[i];
            bool any = g.a.level() == Level::High || g.b.level() == Level::High;
            Level out = any ? Level::Low : Level::High;
            if (pass == 1)
                spdlog::trace("[{}] gate{}: A={} B={} -> Y={}", name(), i+1,
                              int(g.a.level()), int(g.b.level()), int(out));
            g.y.drive(out);
        }
    }
}

} // namespace bench
