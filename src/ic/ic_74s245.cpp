#include "ic/ic_74s245.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S245::IC_74S245() : InlineComponent("74S245") {}

void IC_74S245::on_power_on() {
    driving_ = Driving::None;
}

void IC_74S245::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    // A side: A1=pin2 .. A8=pin9
    for (int i = 0; i < 8; ++i)
        a_[i] = connect_pin(2 + i);

    // B side: B1=pin18, B2=pin17, ..., B8=pin11
    for (int i = 0; i < 8; ++i)
        b_[i] = connect_pin(18 - i);

    g_   = connect_pin(1);   // ~G
    dir_ = connect_pin(19);  // DIR

    Signal* vcc = socket.pin_signal(20);
    if (vcc) vcc->connect(this);

    // Pin directions for wiring visualization.
    declare_input(g_); declare_input(dir_);
    for (int i = 0; i < 8; ++i) { declare_input(a_[i]); declare_output(a_[i]); }
    for (int i = 0; i < 8; ++i) { declare_input(b_[i]); declare_output(b_[i]); }
}

void IC_74S245::on_signal_change(bool /*rising*/, bool /*falling*/) {
    if (g_.level() != Level::Low) {
        release_outputs();
        return;
    }
    bool dir_high = dir_.level() == Level::High;
    uint8_t val = 0;
    if (dir_high) {
        for (int i = 0; i < 8; ++i)
            if (a_[i].level() == Level::High) val |= (1 << i);
    } else {
        for (int i = 0; i < 8; ++i)
            if (b_[i].level() == Level::High) val |= (1 << i);
    }
    update_outputs();
}

void IC_74S245::update_outputs() {
    if (dir_.level() == Level::High) {
        // A -> B: release A if we were driving it
        if (driving_ == Driving::A) {
            for (int i = 0; i < 8; ++i)
                a_[i].release();
        }
        for (int i = 0; i < 8; ++i)
            b_[i].drive(a_[i].level());
        driving_ = Driving::B;
    } else {
        // B -> A: release B if we were driving it
        if (driving_ == Driving::B) {
            for (int i = 0; i < 8; ++i)
                b_[i].release();
        }
        for (int i = 0; i < 8; ++i)
            a_[i].drive(b_[i].level());
        driving_ = Driving::A;
    }
}

void IC_74S245::release_outputs() {
    switch (driving_) {
        case Driving::A:
            for (int i = 0; i < 8; ++i) a_[i].release();
            break;
        case Driving::B:
            for (int i = 0; i < 8; ++i) b_[i].release();
            break;
        case Driving::None:
            break;
    }
    driving_ = Driving::None;
}

} // namespace bench
