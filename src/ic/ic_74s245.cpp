#include "ic/ic_74s245.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S245::IC_74S245() : Component("74S245") {}

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

    if (!enabled) {
        release_all();
        return;
    }

    update_outputs();
}

void IC_74S245::update_outputs() {
    bool a_to_b = pin_dir_ && pin_dir_->level() == Level::High;

    if (a_to_b) {
        // Drive B from A, release A
        for (int i = 0; i < 8; ++i) {
            if (pin_b_[i])
                pin_b_[i]->drive(pin_a_[i] ? pin_a_[i]->level() : Level::HiZ);
        }
    } else {
        // Drive A from B, release B
        for (int i = 0; i < 8; ++i) {
            if (pin_a_[i])
                pin_a_[i]->drive(pin_b_[i] ? pin_b_[i]->level() : Level::HiZ);
        }
    }
}

void IC_74S245::release_all() {
    for (int i = 0; i < 8; ++i) {
        if (pin_a_[i]) pin_a_[i]->release();
        if (pin_b_[i]) pin_b_[i]->release();
    }
}

} // namespace bench
