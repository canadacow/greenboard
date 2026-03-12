#include "ic/ic_74s373.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstring>

namespace bench {

IC_74S373::IC_74S373() : InlineComponent("74S373") {}

void IC_74S373::install(Socket& socket) {
    // D inputs (read current)
    static constexpr int d_pins[] = {3, 4, 7, 8, 13, 14, 17, 18};
    for (int i = 0; i < 8; ++i) {
        Signal* s = socket.pin_signal(d_pins[i]);
        if (s) { s->connect(this); d_[i] = s->pin(); }
    }

    // Q outputs (write pending) -- must be contiguous pool slots
    static constexpr int q_pins[] = {2, 5, 6, 9, 12, 15, 16, 19};
    q_ = PinBlock<8>::from_socket(socket, q_pins);

    // Control signals (read current)
    Signal* le = socket.pin_signal(11);
    Signal* oe = socket.pin_signal(1);
    Signal* vcc = socket.pin_signal(20);

    if (le) { le->connect(this); le_ = le->pin(); }
    if (oe) { oe->connect(this); oe_ = oe->pin(); }
    if (vcc) vcc->connect(this);

    spdlog::debug("[74S373] installed into socket {}", socket.ref());
}

void IC_74S373::on_power_on() {
    for (int i = 0; i < 8; ++i)
        latch_[i] = Level::HiZ;
    le_prev_ = Level::HiZ;
}

void IC_74S373::on_signal_change() {
    Level le = le_.level();

    // LE falling edge: capture D inputs
    if (le == Level::Low && le_prev_ != Level::Low) {
        for (int i = 0; i < 8; ++i)
            latch_[i] = d_[i].level();
    }
    le_prev_ = le;

    // Transparent mode (LE High): latch tracks D
    if (le == Level::High) {
        for (int i = 0; i < 8; ++i)
            latch_[i] = d_[i].level();
    }

    update_outputs();
}

void IC_74S373::update_outputs() {
    if (oe_.level() == Level::Low)
        q_.drive(latch_);
    else
        q_.fill(Level::HiZ);
}

} // namespace bench
