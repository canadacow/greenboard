#include "ic/ic_74s175.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S175::IC_74S175() : CallbackComponent("74S175") { set_description("Quad D Flip-Flop"); }

void IC_74S175::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    pin_clr_ = connect_pin(1);
    pin_clk_ = connect_pin(9);
    pin_vcc_ = connect_pin(16);

    // D inputs: D0=pin4, D1=pin5, D2=pin12, D3=pin13
    pin_d_[0] = connect_pin(4);
    pin_d_[1] = connect_pin(5);
    pin_d_[2] = connect_pin(12);
    pin_d_[3] = connect_pin(13);

    // Q outputs: Q0=pin2, Q1=pin7, Q2=pin10, Q3=pin15
    pin_q_[0] = pin(2);
    pin_q_[1] = pin(7);
    pin_q_[2] = pin(10);
    pin_q_[3] = pin(15);

    // ~Q outputs: ~Q0=pin3, ~Q1=pin6, ~Q2=pin11, ~Q3=pin14
    pin_nq_[0] = pin(3);
    pin_nq_[1] = pin(6);
    pin_nq_[2] = pin(11);
    pin_nq_[3] = pin(14);

    // Pin directions for DAG ordering.
    // D pins default to async (registered FF: no same-cycle combinational path).
    // Use set_d_sync() for specific D pins where the source doesn't create a cycle
    // and correct DAG ordering is needed (e.g. U98 D1=HOLDA from U67).
    declare_input(pin_clr_); declare_input(pin_clk_);
    for (int i = 0; i < 4; ++i) declare_async_input(pin_d_[i]);
    for (int i = 0; i < 4; ++i) { declare_output(pin_q_[i]); declare_output(pin_nq_[i]); }
}

void IC_74S175::set_d_sync(int index) {
    declare_input(pin_d_[index]);
}

void IC_74S175::on_power_on() {
    // Drive initial state (all Q=Low after power-on/reset).
    clear_all();
}

void IC_74S175::on_power_off() {
    for (int i = 0; i < 4; ++i) { pin_q_[i].release(); pin_nq_[i].release(); }
}

void IC_74S175::on_cycle(Fiber /*caller*/) {
    // ~CLR: async clear when driven Low.
    Level cur = pin_clr_.level();
    if (cur == Level::Low && clr_prev_ != Level::Low)
        clear_all();
    clr_prev_ = cur;

    // Latch D inputs every evaluate cycle (CLK is implicit -- one evaluate = one tick).
    if (pin_clr_.level() != Level::Low) {
        for (int i = 0; i < 4; ++i)
            q_[i] = pin_d_[i].level() == Level::High;
        drive_outputs();
    }

}

void IC_74S175::clear_all() {
    for (int i = 0; i < 4; ++i)
        q_[i] = false;
    drive_outputs();
}

void IC_74S175::drive_outputs() {
    for (int i = 0; i < 4; ++i) {
        pin_q_[i].drive(q_[i] ? Level::High : Level::Low);
        pin_nq_[i].drive(q_[i] ? Level::Low : Level::High);
    }
}

} // namespace bench
