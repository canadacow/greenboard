#include "ic/ic_74s373.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S373::IC_74S373() : Component("74S373") {}

void IC_74S373::install(Socket& socket) {
    // D inputs
    pin_d_[0] = socket.pin_signal(3);
    pin_d_[1] = socket.pin_signal(4);
    pin_d_[2] = socket.pin_signal(7);
    pin_d_[3] = socket.pin_signal(8);
    pin_d_[4] = socket.pin_signal(13);
    pin_d_[5] = socket.pin_signal(14);
    pin_d_[6] = socket.pin_signal(17);
    pin_d_[7] = socket.pin_signal(18);

    // Q outputs
    pin_q_[0] = socket.pin_signal(2);
    pin_q_[1] = socket.pin_signal(5);
    pin_q_[2] = socket.pin_signal(6);
    pin_q_[3] = socket.pin_signal(9);
    pin_q_[4] = socket.pin_signal(12);
    pin_q_[5] = socket.pin_signal(15);
    pin_q_[6] = socket.pin_signal(16);
    pin_q_[7] = socket.pin_signal(19);

    // Control
    pin_oe_  = socket.pin_signal(1);   // ~OE
    pin_le_  = socket.pin_signal(11);  // LE
    pin_vcc_ = socket.pin_signal(20);  // VCC

    // Subscribe to control signals
    if (pin_oe_) pin_oe_->connect(this);
    if (pin_le_) pin_le_->connect(this);
    if (pin_vcc_) pin_vcc_->connect(this);

    // Subscribe to D inputs (needed for transparent mode propagation)
    for (int i = 0; i < 8; ++i)
        if (pin_d_[i]) pin_d_[i]->connect(this);

    spdlog::debug("[74S373] installed into socket {}", socket.ref());
}

void IC_74S373::on_signal_change() {
    if (pin_le_) {
        Level cur = pin_le_->level();
        // LE falling edge: capture D inputs
        if (cur == Level::Low && le_prev_ != Level::Low) {
            for (int i = 0; i < 8; ++i)
                latch_[i] = pin_d_[i] ? pin_d_[i]->level() : Level::HiZ;
        }
        le_prev_ = cur;
    }

    // Transparent mode (LE High): update latch from current D levels
    if (pin_le_ && pin_le_->level() == Level::High) {
        for (int i = 0; i < 8; ++i)
            latch_[i] = pin_d_[i] ? pin_d_[i]->level() : Level::HiZ;
    }

    update_outputs();
}

void IC_74S373::update_outputs() {
    bool oe_active = pin_oe_ && pin_oe_->level() == Level::Low;

    if (oe_active) {
        for (int i = 0; i < 8; ++i)
            if (pin_q_[i]) pin_q_[i]->drive(latch_[i]);
    } else {
        for (int i = 0; i < 8; ++i)
            if (pin_q_[i]) pin_q_[i]->release();
    }
}

} // namespace bench
