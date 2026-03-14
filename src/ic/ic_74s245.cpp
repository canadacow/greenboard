#include "ic/ic_74s245.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S245::IC_74S245() : InlineComponent("74S245") { set_description("Bus Transceiver"); }

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

    // A-side and B-side are anti-correlated:
    //   DIR=High -> A->B (A is input, B is output)
    //   DIR=Low  -> B->A (A is output, B is input)
    // Paired as a single bidir block: is_output=true means A drives (DIR=Low).
    declare_bidir_pair(
        {a_[0], a_[1], a_[2], a_[3], a_[4], a_[5], a_[6], a_[7]},   // out_pins (A drives when DIR=Low)
        {b_[0], b_[1], b_[2], b_[3], b_[4], b_[5], b_[6], b_[7]},   // in_pins  (B drives when DIR=High)
        BidirDir::HiZ | BidirDir::Input | BidirDir::Output,
        [this]() -> BidirDir {
            if (g_.level() != Level::Low) return BidirDir::HiZ;
            return dir_.level() == Level::Low ? BidirDir::Output : BidirDir::Input;
        });
}

void IC_74S245::on_signal_change(Fiber /*caller*/, bool /*rising*/, bool /*falling*/) {
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
