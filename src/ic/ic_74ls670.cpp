#include "ic/ic_74ls670.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74LS670::IC_74LS670() : CallbackComponent("74LS670") { set_description("4x4 Reg File"); }

void IC_74LS670::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    // Data inputs: D0=pin15, D1=pin1, D2=pin2, D3=pin3
    pin_d_[0] = connect_pin(15);
    pin_d_[1] = connect_pin(1);
    pin_d_[2] = connect_pin(2);
    pin_d_[3] = connect_pin(3);

    // Data outputs: Q0=pin10, Q1=pin9, Q2=pin7, Q3=pin6
    pin_q_[0] = pin(10);
    pin_q_[1] = pin(9);
    pin_q_[2] = pin(7);
    pin_q_[3] = pin(6);

    // Read address: RA=pin4, RB=pin5
    pin_ra_ = connect_pin(4);
    pin_rb_ = connect_pin(5);

    // Write address: WA=pin14, WB=pin13
    pin_wa_ = connect_pin(14);
    pin_wb_ = connect_pin(13);

    // Enables: ~RE=pin11, ~WE=pin12
    pin_re_ = connect_pin(11);
    pin_we_ = connect_pin(12);

    // VCC
    Signal* vcc = socket.pin_signal(16);
    if (vcc) vcc->connect(this);

    // Pin directions
    if (async_d_)
        for (auto& p : pin_d_) declare_async_input(p);
    else
        for (auto& p : pin_d_) declare_input(p);
    for (auto& p : pin_q_) declare_output(p);
    declare_input(pin_ra_); declare_input(pin_rb_);
    declare_input(pin_wa_); declare_input(pin_wb_);
    declare_input(pin_re_); declare_input(pin_we_);
}

void IC_74LS670::on_power_on() { driving_ = false; }

void IC_74LS670::on_power_off() {
    if (driving_) {
        for (auto& p : pin_q_) p.release();
        driving_ = false;
    }
}

void IC_74LS670::on_signal_change(Fiber /*caller*/) { update(); }

void IC_74LS670::update() {
    // Write: transparent when ~WE=Low
    if (pin_we_.level() == Level::Low) {
        int wa = ((pin_wb_.level() == Level::High) ? 2 : 0)
               | ((pin_wa_.level() == Level::High) ? 1 : 0);
        uint8_t data = 0;
        for (int i = 0; i < 4; ++i) {
            if (pin_d_[i].level() == Level::High)
                data |= (1 << i);
        }
        regs_[wa] = data;
    }

    // Read: outputs driven when ~RE=Low, tri-stated when High
    if (pin_re_.level() == Level::Low) {
        int ra = ((pin_rb_.level() == Level::High) ? 2 : 0)
               | ((pin_ra_.level() == Level::High) ? 1 : 0);
        uint8_t data = regs_[ra];
        // Simultaneous read/write to same address: output reflects inputs
        if (pin_we_.level() == Level::Low) {
            int wa = ((pin_wb_.level() == Level::High) ? 2 : 0)
                   | ((pin_wa_.level() == Level::High) ? 1 : 0);
            if (ra == wa) {
                data = 0;
                for (int i = 0; i < 4; ++i) {
                    if (pin_d_[i].level() == Level::High)
                        data |= (1 << i);
                }
            }
        }
        for (int i = 0; i < 4; ++i)
            pin_q_[i].drive((data >> i) & 1 ? Level::High : Level::Low);
        driving_ = true;
    } else if (driving_) {
        for (auto& p : pin_q_) p.release();
        driving_ = false;
    }
}

} // namespace bench
