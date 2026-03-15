#include "ic/ic_td1.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_TD1::IC_TD1() : CallbackComponent("TD1") { set_description("Delay Line"); }

void IC_TD1::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    pin_in_    = connect_pin(1);   // RAS
    pin_out_[0] = pin(8);          // N-000258
    pin_out_[1] = pin(10);         // ADDR_SEL
    pin_out_[2] = pin(12);         // N-000253

    // VCC
    Signal* vcc = socket.pin_signal(14);
    if (vcc) vcc->connect(this);

    declare_async_input(pin_in_);   // TD1 is a delay line: reads previous cycle's RAS
    for (auto& p : pin_out_) declare_output(p);
}

void IC_TD1::connect_clk(Signal& clk) {
    clk.connect(this);
    declare_input(clk.pin());
}

void IC_TD1::on_power_on() {
    pending_ = Level::HiZ;
    for (auto& p : pin_out_) p.drive(Level::HiZ);
}

void IC_TD1::on_power_off() {
    pending_ = Level::HiZ;
    for (auto& p : pin_out_) p.release();
}

void IC_TD1::on_signal_change(Fiber /*caller*/) {
    Level in_now = pin_in_.level();
    spdlog::trace("[TD1] eval: in={} pending={} -> driving pending, capturing in",
        in_now == Level::High ? "H" : in_now == Level::Low ? "L" : "Z",
        pending_ == Level::High ? "H" : pending_ == Level::Low ? "L" : "Z");
    // Drive outputs with previously captured value.
    for (auto& p : pin_out_) p.drive(pending_);
    // Capture current input for next evaluation.
    pending_ = in_now;
}

} // namespace bench
