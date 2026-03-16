#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S20 Dual 4-Input NAND Gate.
//
// 14-pin DIP. U64 on the 5150 (ROM address decode).
//
// Pinout:
//   Pin  1: A1       (gate 1 input)
//   Pin  2: B1       (gate 1 input)
//   Pin  3: NC
//   Pin  4: C1       (gate 1 input)
//   Pin  5: D1       (gate 1 input)
//   Pin  6: Y1       (gate 1 output)
//   Pin  7: GND
//   Pin  8: Y2       (gate 2 output)
//   Pin  9: A2       (gate 2 input)
//   Pin 10: B2       (gate 2 input)
//   Pin 11: NC
//   Pin 12: C2       (gate 2 input)
//   Pin 13: D2       (gate 2 input)
//   Pin 14: VCC
//
// Behavior (per gate):
//   Y = ~(A & B & C & D)
//   Output is Low only when all four inputs are High.
//
// Template parameter MASK: bit 0 = gate 1, bit 1 = gate 2.
// Only gates with their bit set are evaluated and driven.
//
// Threading: CallbackComponent -- combinational, no fiber.
template<uint8_t MASK = 0x03>
class IC_74S20 : public CallbackComponent {
public:
    IC_74S20() : CallbackComponent("74S20") { set_description("Dual 4-In NAND"); }

    void install(Socket& socket) {
        auto connect_pin = [&](int p) -> Pin {
            Signal* s = socket.pin_signal(p);
            if (s) s->connect(this);
            return s ? s->pin() : Pin{};
        };
        auto pin = [&](int p) -> Pin {
            Signal* s = socket.pin_signal(p);
            return s ? s->pin() : Pin{};
        };

        if constexpr (MASK & 0x01) {
            a1_ = connect_pin(1);
            b1_ = connect_pin(2);
            c1_ = connect_pin(4);
            d1_ = connect_pin(5);
            y1_ = pin(6);
            declare_input(a1_); declare_input(b1_); declare_input(c1_); declare_input(d1_);
            declare_output(y1_);
        }

        if constexpr (MASK & 0x02) {
            a2_ = connect_pin(9);
            b2_ = connect_pin(10);
            c2_ = connect_pin(12);
            d2_ = connect_pin(13);
            y2_ = pin(8);
            declare_input(a2_); declare_input(b2_); declare_input(c2_); declare_input(d2_);
            declare_output(y2_);
        }

        // VCC
        Signal* vcc = socket.pin_signal(14);
        if (vcc) vcc->connect(this);
    }

protected:
    void on_power_on() override { update_outputs(); }

    void on_power_off() override {
        if constexpr (MASK & 0x01) y1_.release();
        if constexpr (MASK & 0x02) y2_.release();
    }

    void on_signal_change(Fiber /*caller*/) override { update_outputs(); }

private:
    void update_outputs() {
        if constexpr (MASK & 0x01) {
            bool all1 =
                a1_.level() == Level::High &&
                b1_.level() == Level::High &&
                c1_.level() == Level::High &&
                d1_.level() == Level::High;
            y1_.drive(all1 ? Level::Low : Level::High);
        }
        if constexpr (MASK & 0x02) {
            bool all2 =
                a2_.level() == Level::High &&
                b2_.level() == Level::High &&
                c2_.level() == Level::High &&
                d2_.level() == Level::High;
            y2_.drive(all2 ? Level::Low : Level::High);
        }
    }

    Pin a1_, b1_, c1_, d1_, y1_;
    Pin a2_, b2_, c2_, d2_, y2_;
};

} // namespace bench
