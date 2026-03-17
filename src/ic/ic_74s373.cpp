#include "ic/ic_74s373.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstring>

namespace bench {

IC_74S373::IC_74S373() : CallbackComponent("74S373") { set_description("Octal Latch"); }

void IC_74S373::install(Socket& socket) {
    static constexpr int d_pins[] = {3, 4, 7, 8, 13, 14, 17, 18};
    static constexpr int q_pins[] = {2, 5, 6, 9, 12, 15, 16, 19};

    d_ = PinBlock<8>::from_socket(socket, d_pins);
    q_ = PinBlock<8>::from_socket(socket, q_pins);

    // Subscribe to D input signals.
    for (int pin : d_pins) {
        Signal* s = socket.pin_signal(pin);
        if (s) s->connect(this);
    }

    // Control signals.
    Signal* le = socket.pin_signal(11);
    Signal* oe = socket.pin_signal(1);
    Signal* vcc = socket.pin_signal(20);

    if (le) { le->connect(this); le_ = le->pin(); }
    if (oe) { oe->connect(this); oe_ = oe->pin(); }
    if (vcc) vcc->connect(this);

    // Pin directions for wiring visualization.
    for (int i = 0; i < 8; ++i) {
        Pin p{d_.base + i};
        if (async_d_) declare_async_input(p); else declare_input(p);
    }
    // LE and ~OE feed bidir lambdas (sampled at permutation time), async.
    declare_async_input(le_); declare_async_input(oe_);
    for (int i = 0; i < 8; ++i) { declare_input(Pin{q_.base + i}); declare_output(Pin{q_.base + i}); }

    // Q outputs are only driven when ~OE=Low.
    // When ~OE=High, outputs are tri-stated (HiZ) -- no DAG dependency.
    {
        Pin q0{q_.base}, q1{q_.base+1}, q2{q_.base+2}, q3{q_.base+3};
        Pin q4{q_.base+4}, q5{q_.base+5}, q6{q_.base+6}, q7{q_.base+7};
        declare_bidir_block({q0, q1, q2, q3, q4, q5, q6, q7},
                            BidirDir::HiZ | BidirDir::Output,
                            [this]() { return oe_.level() == Level::Low ? BidirDir::Output : BidirDir::HiZ; });
    }

    // D inputs are only active when LE=High (transparent mode).
    // When LE=Low (latched), D inputs are disconnected -- no DAG dependency.
    if (!async_d_) {
        Pin d0{d_.base}, d1{d_.base+1}, d2{d_.base+2}, d3{d_.base+3};
        Pin d4{d_.base+4}, d5{d_.base+5}, d6{d_.base+6}, d7{d_.base+7};
        declare_bidir_block({d0, d1, d2, d3, d4, d5, d6, d7},
                            BidirDir::HiZ | BidirDir::Input,
                            [this]() { return le_.level() == Level::High ? BidirDir::Input : BidirDir::HiZ; });
    }
}

void IC_74S373::on_power_on() {
    std::memset(latch_, static_cast<uint8_t>(Level::HiZ), 8);
    le_prev_ = Level::HiZ;
    oe_active_ = false;
}

void IC_74S373::on_signal_change(Fiber /*caller*/) {
    Level le = le_.level();

    // LE falling edge: stop tracking. latch_[] already holds the last
    // transparent-mode value -- no re-read of D pins (they may have
    // changed, e.g. AD0-AD7 released by 8088 after T1).
    le_prev_ = le;

    // Transparent mode: Q tracks D continuously.
    if (le == Level::High) {
        d_.read(latch_);
    }

    update_outputs();
}

void IC_74S373::update_outputs() {
    bool oe_low = oe_.level() == Level::Low;
    if (oe_low) {
        q_.drive(latch_);
        oe_active_ = true;
    } else if (oe_active_) {
        q_.release();
        oe_active_ = false;
    }
}

} // namespace bench
