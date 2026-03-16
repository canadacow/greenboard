#include "ic/ic_74s00_u81.h"
#include <spdlog/spdlog.h>

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

    // Gate 3: pins 9,10 -> 8   (~CAS output)
    // Physical pin inputs ignored -- gate 3 uses virtual TD1 (delayed RAS).
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

void IC_74S00_U81::connect_addr_sel(Signal& addr_sel) {
    addr_sel_pin_ = addr_sel.pin();
    declare_output(addr_sel_pin_);
}

void IC_74S00_U81::on_power_on() {
    td1_pending_ = Level::HiZ;
    if (addr_sel_pin_.idx != 0) addr_sel_pin_.drive(Level::HiZ);
    for (auto& g : gates_) {
        bool both = g.a.level() == Level::High && g.b.level() == Level::High;
        g.y.drive(both ? Level::Low : Level::High);
    }
}

void IC_74S00_U81::on_power_off() {
    td1_pending_ = Level::HiZ;
    if (addr_sel_pin_.idx != 0) addr_sel_pin_.release();
    for (auto& g : gates_)
        g.y.release();
}

void IC_74S00_U81::on_signal_change(Fiber /*caller*/) {
    // Gate 2 first (drives RAS)
    {
        auto& g = gates_[1];
        Level a = g.a.level(), b = g.b.level();
        bool both = a == Level::High && b == Level::High;
        Level y = both ? Level::Low : Level::High;
        spdlog::trace("[U81] gate2: ~MEMR={} ~MEMW={} -> RAS={}", (int)a, (int)b, (int)y);
        g.y.drive(y);
    }

    // Gate 3: virtual TD1 -- use delayed RAS, not pin inputs
    {
        Level ras_now = gates_[1].y.level();
        if (addr_sel_pin_.idx != 0) {
            spdlog::trace("[U81] addr_sel <- td1_pending={}", (int)td1_pending_);
            addr_sel_pin_.drive(td1_pending_);
        }
        Level out = (td1_pending_ == Level::High) ? Level::Low : Level::High;
        spdlog::trace("[U81] gate3(CAS): td1_pending={} -> ~CAS={}", (int)td1_pending_, (int)out);
        gates_[2].y.drive(out);
        td1_pending_ = ras_now;
    }

    // Gates 1 and 4
    for (int i : {0, 3}) {
        auto& g = gates_[i];
        bool both = g.a.level() == Level::High && g.b.level() == Level::High;
        g.y.drive(both ? Level::Low : Level::High);
    }
}

} // namespace bench
