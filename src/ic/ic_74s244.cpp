#include "ic/ic_74s244.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S244::IC_74S244() : CallbackComponent("74S244") { set_description("Octal Buffer"); }

void IC_74S244::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    // Enable pins
    pin_g1_ = connect_pin(1);   // ~1G (group 1 enable, active low)
    pin_g2_ = connect_pin(19);  // ~2G (group 2 enable, active low)

    // Group 1: inputs (2,4,6,8) -> outputs (18,16,14,12)
    grp1_[0].a = connect_pin(2);  grp1_[0].y = pin(18);  // 1A1 -> 1Y1
    grp1_[1].a = connect_pin(4);  grp1_[1].y = pin(16);  // 1A2 -> 1Y2
    grp1_[2].a = connect_pin(6);  grp1_[2].y = pin(14);  // 1A3 -> 1Y3
    grp1_[3].a = connect_pin(8);  grp1_[3].y = pin(12);  // 1A4 -> 1Y4

    // Group 2: inputs (11,13,15,17) -> outputs (9,7,5,3)
    grp2_[0].a = connect_pin(11); grp2_[0].y = pin(9);   // 2A1 -> 2Y1
    grp2_[1].a = connect_pin(13); grp2_[1].y = pin(7);   // 2A2 -> 2Y2
    grp2_[2].a = connect_pin(15); grp2_[2].y = pin(5);   // 2A3 -> 2Y3
    grp2_[3].a = connect_pin(17); grp2_[3].y = pin(3);   // 2A4 -> 2Y4

    // VCC
    Signal* vcc = socket.pin_signal(20);
    if (vcc) vcc->connect(this);

    // Pin directions
    declare_input(pin_g1_);
    declare_input(pin_g2_);
    for (auto& b : grp1_) { declare_input(b.a); declare_output(b.y); }
    for (auto& b : grp2_) { declare_input(b.a); declare_output(b.y); }
}

void IC_74S244::on_power_on() {
    update_outputs();
}

void IC_74S244::on_power_off() {
    for (auto& b : grp1_) b.y.release();
    for (auto& b : grp2_) b.y.release();
}

void IC_74S244::on_signal_change(Fiber /*caller*/) {
    update_outputs();
}

void IC_74S244::update_outputs() {
    bool g1_en = pin_g1_.level() == Level::Low;
    bool g2_en = pin_g2_.level() == Level::Low;

    for (auto& b : grp1_) {
        if (g1_en)
            b.y.drive(b.a.level() == Level::High ? Level::High : Level::Low);
        else
            b.y.release();
    }
    for (auto& b : grp2_) {
        if (g2_en)
            b.y.drive(b.a.level() == Level::High ? Level::High : Level::Low);
        else
            b.y.release();
    }
}

} // namespace bench
