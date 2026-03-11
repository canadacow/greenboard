#include "ic/ic_74s175.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S175::IC_74S175() : ThreadedComponent("74S175") {}

void IC_74S175::install(Socket& socket) {
    pin_clr_ = socket.pin_signal(1);    // ~CLR
    pin_clk_ = socket.pin_signal(9);    // CLK
    pin_vcc_ = socket.pin_signal(16);   // VCC

    // D inputs
    pin_d_[0] = socket.pin_signal(4);   // 1D
    pin_d_[1] = socket.pin_signal(5);   // 2D
    pin_d_[2] = socket.pin_signal(12);  // 3D
    pin_d_[3] = socket.pin_signal(15);  // 4D

    // Q outputs
    pin_q_[0] = socket.pin_signal(2);   // 1Q
    pin_q_[1] = socket.pin_signal(7);   // 2Q
    pin_q_[2] = socket.pin_signal(10);  // 3Q
    pin_q_[3] = socket.pin_signal(14);  // 4Q

    // ~Q outputs
    pin_nq_[0] = socket.pin_signal(3);  // ~1Q
    pin_nq_[1] = socket.pin_signal(6);  // ~2Q
    pin_nq_[2] = socket.pin_signal(11); // ~3Q
    pin_nq_[3] = socket.pin_signal(13); // ~4Q

    // Subscribe to clock and clear.
    if (pin_clk_) pin_clk_->connect(this);
    if (pin_clr_) pin_clr_->connect(this);
    if (pin_vcc_) pin_vcc_->connect(this);

    spdlog::debug("[74S175] installed into socket {}", socket.ref());
}

void IC_74S175::on_signal_change() {
    // ~CLR: async clear when driven Low.
    if (pin_clr_) {
        Level cur = pin_clr_->level();
        if (cur == Level::Low && clr_prev_ != Level::Low)
            clear_all();
        clr_prev_ = cur;
    }

    // CLK rising edge: latch D inputs.
    if (pin_clk_) {
        Level cur = pin_clk_->level();
        if (cur == Level::High && clk_prev_ != Level::High) {
            // Only latch if ~CLR is not asserted.
            if (!pin_clr_ || pin_clr_->level() != Level::Low)
                on_clk_rising();
        }
        clk_prev_ = cur;
    }
}

void IC_74S175::on_clk_rising() {
    // Sample all D inputs and update Q state.
    for (int i = 0; i < 4; ++i) {
        q_[i] = pin_d_[i] && pin_d_[i]->level() == Level::High;
    }
    drive_outputs();
}

void IC_74S175::clear_all() {
    for (int i = 0; i < 4; ++i)
        q_[i] = false;
    drive_outputs();
}

void IC_74S175::drive_outputs() {
    for (int i = 0; i < 4; ++i) {
        if (pin_q_[i])  pin_q_[i]->drive(q_[i] ? Level::High : Level::Low);
        if (pin_nq_[i]) pin_nq_[i]->drive(q_[i] ? Level::Low : Level::High);
    }
}

} // namespace bench
