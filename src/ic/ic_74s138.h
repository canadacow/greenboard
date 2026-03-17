#pragma once
#include "core/callback_component.h"
#include "board/socket.h"
#include <spdlog/spdlog.h>
#include <bit>
#include <array>

namespace bench {

// 74S138 3-to-8 Line Decoder/Demultiplexer.
//
// 16-pin DIP. U46, U47, U48, U65, U66 on the 5150 (address decode).
//
// Template parameter MASK: bitmask of active outputs (bit 0 = ~Y0, bit 7 = ~Y7).
// Only active outputs are driven; the compiler unrolls to exactly popcount(MASK)
// drive() calls per evaluation.
//
// Standard pinout:
//   Pin  1: A        (select input)
//   Pin  2: B        (select input)
//   Pin  3: C        (select input)
//   Pin  4: ~G2A     (enable, active low)
//   Pin  5: ~G2B     (enable, active low)
//   Pin  6: G1       (enable, active high)
//   Pin  7: ~Y7      (output, active low)
//   Pin  8: GND
//   Pin  9: ~Y6
//   Pin 10: ~Y5
//   Pin 11: ~Y4
//   Pin 12: ~Y3
//   Pin 13: ~Y2
//   Pin 14: ~Y1
//   Pin 15: ~Y0
//   Pin 16: VCC
//
// Behavior:
//   When G1=High AND ~G2A=Low AND ~G2B=Low:
//     The output selected by CBA (0-7) goes Low, all others High.
//   Otherwise:
//     All outputs High (inactive).
template<uint8_t MASK = 0xFF>
class IC_74S138 : public CallbackComponent {
public:
    IC_74S138() : CallbackComponent("74S138") { set_description("3-to-8 Decoder"); }

    void install(Socket& socket) {
        auto pin = [&](int p) -> Pin {
            Signal* s = socket.pin_signal(p);
            return s ? s->pin() : Pin{};
        };
        auto connect_pin = [&](int p) -> Pin {
            Signal* s = socket.pin_signal(p);
            if (s) s->connect(this);
            return s ? s->pin() : Pin{};
        };

        a_   = connect_pin(1);
        b_   = connect_pin(2);
        c_   = connect_pin(3);
        g2a_ = connect_pin(4);
        g2b_ = connect_pin(5);
        g1_  = connect_pin(6);

        // Outputs: ~Y0=pin15, ~Y1=pin14, ..., ~Y7=pin7
        static constexpr int y_pins[] = {15, 14, 13, 12, 11, 10, 9, 7};
        for (int i = 0; i < 8; ++i)
            y_[i] = pin(y_pins[i]);

        // VCC
        Signal* vcc = socket.pin_signal(16);
        if (vcc) vcc->connect(this);

        // Pin directions
        declare_input(a_); declare_input(b_); declare_input(c_);
        declare_input(g2a_); declare_input(g2b_); declare_input(g1_);
        for (int i : active_)
            declare_output(y_[i]);
    }

protected:
    void on_power_on() override {
        for (int i : active_)
            y_[i].drive(Level::High);
    }

    void on_power_off() override {
        for (int i : active_)
            y_[i].release();
    }

    void on_signal_change(Fiber /*caller*/) override {
        update_outputs();
    }

private:
    static constexpr auto active_ = []() {
        std::array<int, std::popcount(MASK)> a{};
        int n = 0;
        for (int i = 0; i < 8; ++i)
            if (MASK & (1 << i)) a[n++] = i;
        return a;
    }();

    void update_outputs() {
        bool enabled =
            g1_.level()  == Level::High &&
            g2a_.level() == Level::Low &&
            g2b_.level() == Level::Low;

        if (enabled) {
            int sel = 0;
            if (a_.level() == Level::High) sel |= 1;
            if (b_.level() == Level::High) sel |= 2;
            if (c_.level() == Level::High) sel |= 4;

            for (int i : active_)
                y_[i].drive(i == sel ? Level::Low : Level::High);
            spdlog::trace("[{}] enabled sel={} G1={} ~G2A={} ~G2B={}", name(), sel,
                          int(g1_.level()), int(g2a_.level()), int(g2b_.level()));
        } else {
            for (int i : active_)
                y_[i].drive(Level::High);
            spdlog::trace("[{}] disabled G1={} ~G2A={} ~G2B={}", name(),
                          int(g1_.level()), int(g2a_.level()), int(g2b_.level()));
        }
    }

    // Select inputs
    Pin a_;     // Pin 1: A (LSB)
    Pin b_;     // Pin 2: B
    Pin c_;     // Pin 3: C (MSB)

    // Enable inputs
    Pin g2a_;   // Pin 4: ~G2A (active low)
    Pin g2b_;   // Pin 5: ~G2B (active low)
    Pin g1_;    // Pin 6: G1 (active high)

    // Outputs (~Y0 through ~Y7, active low)
    Pin y_[8];
};

} // namespace bench
