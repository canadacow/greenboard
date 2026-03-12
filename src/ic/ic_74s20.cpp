#include "ic/ic_74s20.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S20::IC_74S20() : InlineComponent("74S20") {}

void IC_74S20::install(Socket& socket) {
    // Gate 1
    pin_a1_ = socket.pin_signal(1);
    pin_b1_ = socket.pin_signal(2);
    pin_c1_ = socket.pin_signal(4);   // Pin 3 is NC
    pin_d1_ = socket.pin_signal(5);
    pin_y1_ = socket.pin_signal(6);

    // Gate 2
    pin_a2_ = socket.pin_signal(9);
    pin_b2_ = socket.pin_signal(10);
    pin_c2_ = socket.pin_signal(12);  // Pin 11 is NC
    pin_d2_ = socket.pin_signal(13);
    pin_y2_ = socket.pin_signal(8);

    pin_vcc_ = socket.pin_signal(14);

    // Subscribe to all inputs.
    if (pin_a1_) pin_a1_->connect(this);
    if (pin_b1_) pin_b1_->connect(this);
    if (pin_c1_) pin_c1_->connect(this);
    if (pin_d1_) pin_d1_->connect(this);
    if (pin_a2_) pin_a2_->connect(this);
    if (pin_b2_) pin_b2_->connect(this);
    if (pin_c2_) pin_c2_->connect(this);
    if (pin_d2_) pin_d2_->connect(this);
    if (pin_vcc_) pin_vcc_->connect(this);

    spdlog::debug("[74S20] installed into socket {}", socket.ref());
}

void IC_74S20::on_power_on() {
    update_outputs();
}

void IC_74S20::on_power_off() {
    if (pin_y1_) pin_y1_->release();
    if (pin_y2_) pin_y2_->release();
}

void IC_74S20::on_signal_change() {
    update_outputs();
}

void IC_74S20::update_outputs() {
    // Gate 1: Y1 = ~(A1 & B1 & C1 & D1)
    if (pin_y1_) {
        bool all_high =
            (pin_a1_ && pin_a1_->level() == Level::High) &&
            (pin_b1_ && pin_b1_->level() == Level::High) &&
            (pin_c1_ && pin_c1_->level() == Level::High) &&
            (pin_d1_ && pin_d1_->level() == Level::High);
        pin_y1_->drive(all_high ? Level::Low : Level::High);
    }

    // Gate 2: Y2 = ~(A2 & B2 & C2 & D2)
    if (pin_y2_) {
        bool all_high =
            (pin_a2_ && pin_a2_->level() == Level::High) &&
            (pin_b2_ && pin_b2_->level() == Level::High) &&
            (pin_c2_ && pin_c2_->level() == Level::High) &&
            (pin_d2_ && pin_d2_->level() == Level::High);
        pin_y2_->drive(all_high ? Level::Low : Level::High);
    }
}

} // namespace bench
