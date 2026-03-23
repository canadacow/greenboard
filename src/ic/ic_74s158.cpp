#include "ic/ic_74s158.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S158::IC_74S158() : CallbackComponent("74S158") { set_description("Quad 2:1 MUX Inv"); }

void IC_74S158::install(Socket& socket) {
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };

    pin_select_ = connect_pin(1);
    pin_strobe_ = connect_pin(15);

    // Mux A: pins 2(I0), 3(I1) -> 4(Y)
    muxes_[0].i0 = connect_pin(2);
    muxes_[0].i1 = connect_pin(3);
    muxes_[0].y  = pin(4);

    // Mux B: pins 5(I0), 6(I1) -> 7(Y)
    muxes_[1].i0 = connect_pin(5);
    muxes_[1].i1 = connect_pin(6);
    muxes_[1].y  = pin(7);

    // Mux C: pins 11(I0), 10(I1) -> 9(Y)
    muxes_[2].i0 = connect_pin(11);
    muxes_[2].i1 = connect_pin(10);
    muxes_[2].y  = pin(9);

    // Mux D: pins 14(I0), 13(I1) -> 12(Y)
    muxes_[3].i0 = connect_pin(14);
    muxes_[3].i1 = connect_pin(13);
    muxes_[3].y  = pin(12);

    // VCC
    Signal* vcc = socket.pin_signal(16);
    if (vcc) vcc->connect(this);

    // Pin directions
    declare_input(pin_select_);
    declare_input(pin_strobe_);
    for (auto& m : muxes_) {
        declare_input(m.i0);
        declare_input(m.i1);
        declare_output(m.y);
    }
}

void IC_74S158::on_power_on() {
    update_outputs();
}

void IC_74S158::on_power_off() {
    for (auto& m : muxes_)
        m.y.release();
}

void IC_74S158::on_signal_change(Fiber /*caller*/) {
    update_outputs();
}

void IC_74S158::update_outputs() {
    // ~STROBE High -> all outputs High (disabled)
    if (pin_strobe_.level() == Level::High) {
        for (auto& m : muxes_)
            m.y.drive(Level::High);
        return;
    }

    bool sel = pin_select_.level() == Level::High;
    for (int i = 0; i < 4; ++i) {
        auto& m = muxes_[i];
        Level chosen = sel ? m.i1.level() : m.i0.level();
        m.y.drive(chosen == Level::High ? Level::Low : Level::High);
    }

#if 0  // reads partner's pins without declaration -- triggers pin validation
    if (partner_) {
        auto to_byte = [](const Mux* lo, const Mux* hi, bool use_i1) -> uint8_t {
            uint8_t v = 0;
            for (int i = 0; i < 4; ++i) {
                Level l = use_i1 ? lo[i].i1.level() : lo[i].i0.level();
                if (l == Level::High) v |= (1 << i);
            }
            for (int i = 0; i < 4; ++i) {
                Level l = use_i1 ? hi[i].i1.level() : hi[i].i0.level();
                if (l == Level::High) v |= (1 << (i + 4));
            }
            return v;
        };
        uint8_t row_src = to_byte(muxes_, partner_->muxes(), false);
        uint8_t col_src = to_byte(muxes_, partner_->muxes(), true);
        spdlog::debug("[MUX] ADDR_SEL={} row_src(A0-A7)=0x{:02X} col_src(A8-A15)=0x{:02X}",
                      sel ? "COL" : "ROW", row_src, col_src);
    }
#endif
}

} // namespace bench
