#include "ic/ic_74s373.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstring>

namespace bench {

IC_74S373::IC_74S373() : InlineComponent("74S373") {}

void IC_74S373::install(Socket& socket) {
    // D inputs (read current) -- must be contiguous pool slots
    static constexpr int d_pins[] = {3, 4, 7, 8, 13, 14, 17, 18};
    d_ = PinBlock<8>::from_socket(socket, d_pins);
    for (int pin : d_pins) {
        Signal* s = socket.pin_signal(pin);
        if (s) s->connect(this);
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
}

void IC_74S373::on_power_on() {
    std::memset(latch_, static_cast<uint8_t>(Level::HiZ), 8);
    le_prev_ = Level::HiZ;
}

void IC_74S373::on_signal_change(bool rising, bool /*falling*/) {
    if (!rising) return;  // edge-tracking: run once per cycle
    Level le = le_.level();

    // LE falling edge or transparent mode: capture D inputs
    if (le == Level::Low && le_prev_ != Level::Low) {
        d_.read(latch_);
        uint8_t val = 0;
        for (int i = 0; i < 8; ++i)
            if (latch_[i] == Level::High) val |= (1 << i);
    }
    le_prev_ = le;

    if (le == Level::High)
        d_.read(latch_);

    update_outputs();
}

void IC_74S373::update_outputs() {
    if (oe_.level() == Level::Low)
        q_.drive(latch_);
    else
        q_.fill(Level::HiZ);
}

} // namespace bench
