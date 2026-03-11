#include "ic/ic_74s138.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S138::IC_74S138() : Component("74S138") {}

void IC_74S138::install(Socket& socket) {
    // Select inputs
    pin_a_ = socket.pin_signal(1);
    pin_b_ = socket.pin_signal(2);
    pin_c_ = socket.pin_signal(3);

    // Enable inputs
    pin_g2a_ = socket.pin_signal(4);   // ~G2A (active low)
    pin_g2b_ = socket.pin_signal(5);   // ~G2B (active low)
    pin_g1_  = socket.pin_signal(6);   // G1 (active high)

    // Outputs: ~Y0=pin15, ~Y1=pin14, ..., ~Y7=pin7
    pin_y_[0] = socket.pin_signal(15);
    pin_y_[1] = socket.pin_signal(14);
    pin_y_[2] = socket.pin_signal(13);
    pin_y_[3] = socket.pin_signal(12);
    pin_y_[4] = socket.pin_signal(11);
    pin_y_[5] = socket.pin_signal(10);
    pin_y_[6] = socket.pin_signal(9);
    pin_y_[7] = socket.pin_signal(7);

    pin_vcc_ = socket.pin_signal(16);

    // Subscribe to all inputs.
    if (pin_a_)   pin_a_->connect(this);
    if (pin_b_)   pin_b_->connect(this);
    if (pin_c_)   pin_c_->connect(this);
    if (pin_g2a_) pin_g2a_->connect(this);
    if (pin_g2b_) pin_g2b_->connect(this);
    if (pin_g1_)  pin_g1_->connect(this);
    if (pin_vcc_) pin_vcc_->connect(this);

    spdlog::debug("[74S138] installed into socket {}", socket.ref());
}

void IC_74S138::on_power_on() {
    // All outputs High (inactive) on power-up.
    for (int i = 0; i < 8; ++i) {
        if (pin_y_[i])
            pin_y_[i]->drive(Level::High);
    }
}

void IC_74S138::on_power_off() {
    // Release all outputs.
    for (int i = 0; i < 8; ++i) {
        if (pin_y_[i])
            pin_y_[i]->release();
    }
}

void IC_74S138::on_signal_change() {
    update_outputs();
}

void IC_74S138::update_outputs() {
    // Check enables: G1=High, ~G2A=Low, ~G2B=Low
    bool enabled =
        (pin_g1_  && pin_g1_->level()  == Level::High) &&
        (pin_g2a_ && pin_g2a_->level() == Level::Low) &&
        (pin_g2b_ && pin_g2b_->level() == Level::Low);

    if (enabled) {
        // Decode CBA select lines
        int sel = 0;
        if (pin_a_ && pin_a_->level() == Level::High) sel |= 1;
        if (pin_b_ && pin_b_->level() == Level::High) sel |= 2;
        if (pin_c_ && pin_c_->level() == Level::High) sel |= 4;

        // Selected output goes Low, all others High.
        for (int i = 0; i < 8; ++i) {
            if (pin_y_[i])
                pin_y_[i]->drive(i == sel ? Level::Low : Level::High);
        }
    } else {
        // All outputs High (inactive).
        for (int i = 0; i < 8; ++i) {
            if (pin_y_[i])
                pin_y_[i]->drive(Level::High);
        }
    }
}

} // namespace bench
