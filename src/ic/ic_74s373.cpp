#include "ic/ic_74s373.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstring>

namespace bench {

IC_74S373::IC_74S373() : InlineComponent("74S373") { set_description("Octal Latch"); }

void IC_74S373::install(Socket& socket) {
    // D inputs (read current)
    static constexpr int d_pins[] = {3, 4, 7, 8, 13, 14, 17, 18};
    for (int i = 0; i < 8; ++i) {
        Signal* s = socket.pin_signal(d_pins[i]);
        if (s) { s->connect(this); d_[i] = s->pin(); }
    }

    // Q outputs (write pending)
    static constexpr int q_pins[] = {2, 5, 6, 9, 12, 15, 16, 19};
    for (int i = 0; i < 8; ++i) {
        Signal* s = socket.pin_signal(q_pins[i]);
        if (s) q_[i] = s->pin();
    }

    // Control signals (read current)
    Signal* le = socket.pin_signal(11);
    Signal* oe = socket.pin_signal(1);
    Signal* vcc = socket.pin_signal(20);

    if (le) { le->connect(this); le_ = le->pin(); }
    if (oe) { oe->connect(this); oe_ = oe->pin(); }
    if (vcc) vcc->connect(this);

    // Pin directions for wiring visualization.
    for (int i = 0; i < 8; ++i) declare_input(d_[i]);
    declare_input(le_); declare_input(oe_);
    for (int i = 0; i < 8; ++i) declare_output(q_[i]);

    // D inputs are only active when LE=High (transparent mode).
    // When LE=Low (latched), D inputs are disconnected -- no DAG dependency.
    declare_bidir_block({d_[0], d_[1], d_[2], d_[3], d_[4], d_[5], d_[6], d_[7]},
                        BidirDir::HiZ | BidirDir::Input,
                        [this]() { return le_.level() == Level::High ? BidirDir::Input : BidirDir::HiZ; });
}

void IC_74S373::on_power_on() {
    std::memset(latch_, static_cast<uint8_t>(Level::HiZ), 8);
    le_prev_ = Level::HiZ;
}

void IC_74S373::on_signal_change(Fiber /*caller*/) {
    Level le = le_.level();

    // LE falling edge: stop tracking. latch_[] already holds the last
    // transparent-mode value -- no re-read of D pins (they may have
    // changed, e.g. AD0-AD7 released by 8088 after T1).
    le_prev_ = le;

    // Transparent mode: Q tracks D continuously
    if (le == Level::High) {
        for (int i = 0; i < 8; ++i) latch_[i] = d_[i].level();
        uint8_t val = 0;
        for (int i = 0; i < 8; ++i)
            if (latch_[i] == Level::High) val |= (1 << i);
    }

    update_outputs();
}

void IC_74S373::update_outputs() {
    if (oe_.level() == Level::Low)
        for (int i = 0; i < 8; ++i) q_[i].drive(latch_[i]);
    else
        for (int i = 0; i < 8; ++i) q_[i].drive(Level::HiZ);
}

} // namespace bench
