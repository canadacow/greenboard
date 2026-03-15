#include "ic/ic_74s00_u81.h"

namespace bench {

IC_74S00_U81::IC_74S00_U81() : CallbackComponent("74S00") { set_description("Quad 2-In NAND"); }

void IC_74S00_U81::install(Socket& socket) {
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
    gates_[0].b = connect_pin(2);   // RAS (input)
    gates_[0].y = pin(3);

    // Gate 2: pins 4,5 -> 6
    gates_[1].a = connect_pin(4);
    gates_[1].b = connect_pin(5);
    gates_[1].y = pin(6);           // RAS (output)

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

    // Gate 3 inputs (pins 9,10) come from TD1's one-cycle delay.
    // They carry the previous cycle's value -- no same-cycle dependency.
    declare_async_input(gates_[2].a);
    declare_async_input(gates_[2].b);

    // RAS is the shared signal (gate 1 input B = gate 2 output Y).
    ras_pin_ = gates_[0].b;

    // Gate 2 always drives RAS. Tell the scheduler so TD1 orders after U81.
    declare_bidir_block({ras_pin_},
        BidirDir::Output,
        []() { return BidirDir::Output; });
}

void IC_74S00_U81::on_power_on() {
    for (auto& g : gates_) {
        bool both = g.a.level() == Level::High && g.b.level() == Level::High;
        g.y.drive(both ? Level::Low : Level::High);
    }
}

void IC_74S00_U81::on_power_off() {
    for (auto& g : gates_)
        g.y.release();
}

void IC_74S00_U81::on_signal_change(Fiber /*caller*/) {
    for (auto& g : gates_) {
        bool both = g.a.level() == Level::High && g.b.level() == Level::High;
        g.y.drive(both ? Level::Low : Level::High);
    }
}

} // namespace bench
