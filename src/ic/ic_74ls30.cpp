#include "ic/ic_74ls30.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74LS30::IC_74LS30() : CallbackComponent("74LS30") { set_description("8-In NAND"); }

void IC_74LS30::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    // Inputs: pins 1-6 (A-F), 11 (G), 12 (H)
    inputs_[0] = connect_pin(1);   // A
    inputs_[1] = connect_pin(2);   // B
    inputs_[2] = connect_pin(3);   // C
    inputs_[3] = connect_pin(4);   // D
    inputs_[4] = connect_pin(5);   // E
    inputs_[5] = connect_pin(6);   // F
    inputs_[6] = connect_pin(11);  // G
    inputs_[7] = connect_pin(12);  // H

    // Output: pin 8 (Y)
    output_ = pin(8);

    // VCC
    Signal* vcc = socket.pin_signal(14);
    if (vcc) vcc->connect(this);

    // Pin directions
    for (auto& p : inputs_) declare_input(p);
    declare_output(output_);
}

void IC_74LS30::on_power_on() { update_output(); }

void IC_74LS30::on_power_off() { output_.release(); }

void IC_74LS30::on_cycle(Fiber /*caller*/) { update_output(); }

void IC_74LS30::update_output() {
    // Y = ~(A & B & C & D & E & F & G & H)
    bool all_high = true;
    for (int i = 0; i < 8; ++i) {
        if (inputs_[i].level() != Level::High) {
            all_high = false;
            break;
        }
    }
    output_.drive(all_high ? Level::Low : Level::High);
}

} // namespace bench
