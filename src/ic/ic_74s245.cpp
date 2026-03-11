#include "ic/ic_74s245.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S245::IC_74S245() : InlineComponent("74S245") {}

void IC_74S245::on_power_on() {
    driving_ = Driving::None;
}

void IC_74S245::install(Socket& socket) {
    // A side: A1=pin2 .. A8=pin9
    for (int i = 0; i < 8; ++i)
        pin_a_[i] = socket.pin_signal(2 + i);

    // B side: B1=pin18, B2=pin17, ..., B8=pin11 (reversed)
    for (int i = 0; i < 8; ++i)
        pin_b_[i] = socket.pin_signal(18 - i);

    // Control
    pin_g_   = socket.pin_signal(1);   // ~G
    pin_dir_ = socket.pin_signal(19);  // DIR
    pin_vcc_ = socket.pin_signal(20);  // VCC

    // Subscribe to control signals
    if (pin_g_)   pin_g_->connect(this);
    if (pin_dir_) pin_dir_->connect(this);
    if (pin_vcc_) pin_vcc_->connect(this);

    // Subscribe to all data pins (both sides)
    for (int i = 0; i < 8; ++i) {
        if (pin_a_[i]) pin_a_[i]->connect(this);
        if (pin_b_[i]) pin_b_[i]->connect(this);
    }

    spdlog::debug("[74S245] installed into socket {}", socket.ref());
}

void IC_74S245::on_signal_change() {
    bool enabled = pin_g_ && pin_g_->level() == Level::Low;

    spdlog::trace("[74S245] on_signal_change ~G={} DIR={} enabled={} driving={}",
        pin_g_ ? (int)pin_g_->level() : -1,
        pin_dir_ ? (int)pin_dir_->level() : -1,
        enabled,
        driving_ == Driving::None ? "None" : (driving_ == Driving::A ? "A" : "B"));

    if (!enabled) {
        release_outputs();
        return;
    }

    update_outputs();
}

void IC_74S245::update_outputs() {
    bool a_to_b = pin_dir_ && pin_dir_->level() == Level::High;

    if (a_to_b) {
        // Drive B from A. Only release A if we were previously driving it.
        if (driving_ == Driving::A) {
            spdlog::trace("[74S245] direction change A->B, releasing A side");
            for (int i = 0; i < 8; ++i)
                if (pin_a_[i]) pin_a_[i]->release();
        }
        uint8_t a_val = 0;
        for (int i = 0; i < 8; ++i)
            if (pin_a_[i] && pin_a_[i]->level() == Level::High) a_val |= (1 << i);
        for (int i = 0; i < 8; ++i) {
            if (pin_b_[i])
                pin_b_[i]->drive(pin_a_[i] ? pin_a_[i]->level() : Level::HiZ);
        }
        spdlog::trace("[74S245] A->B: A=0x{:02X} driven onto B", a_val);
        driving_ = Driving::B;
    } else {
        // Drive A from B. Only release B if we were previously driving it.
        if (driving_ == Driving::B) {
            spdlog::trace("[74S245] direction change B->A, releasing B side");
            for (int i = 0; i < 8; ++i)
                if (pin_b_[i]) pin_b_[i]->release();
        }
        uint8_t b_val = 0;
        for (int i = 0; i < 8; ++i)
            if (pin_b_[i] && pin_b_[i]->level() == Level::High) b_val |= (1 << i);
        for (int i = 0; i < 8; ++i) {
            if (pin_a_[i])
                pin_a_[i]->drive(pin_b_[i] ? pin_b_[i]->level() : Level::HiZ);
        }
        spdlog::trace("[74S245] B->A: B=0x{:02X} driven onto A (AD)", b_val);
        driving_ = Driving::A;
    }
}

void IC_74S245::release_outputs() {
    spdlog::trace("[74S245] release_outputs driving={}",
        driving_ == Driving::None ? "None" : (driving_ == Driving::A ? "A" : "B"));
    switch (driving_) {
        case Driving::A:
            for (int i = 0; i < 8; ++i)
                if (pin_a_[i]) pin_a_[i]->release();
            break;
        case Driving::B:
            for (int i = 0; i < 8; ++i)
                if (pin_b_[i]) pin_b_[i]->release();
            break;
        case Driving::None:
            break;
    }
    driving_ = Driving::None;
}

} // namespace bench
