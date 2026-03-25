#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S244 Octal Tri-State Buffer (Schottky, non-inverting).
//
// 20-pin DIP. U15, U16, U17, U23 on the 5150.
//
// Two independent groups of 4 buffers, each with its own ~G (enable).
// Interleaved pinout: group 1 inputs on top-left, outputs on bottom-right,
// group 2 inputs on bottom-left, outputs on top-right.
//
// Pinout:
//   Pin  1: ~1G     (group 1 enable, active low)
//   Pin  2: 1A1     (group 1 input)
//   Pin  3: 2Y4     (group 2 output)
//   Pin  4: 1A2     (group 1 input)
//   Pin  5: 2Y3     (group 2 output)
//   Pin  6: 1A3     (group 1 input)
//   Pin  7: 2Y2     (group 2 output)
//   Pin  8: 1A4     (group 1 input)
//   Pin  9: 2Y1     (group 2 output)
//   Pin 10: GND
//   Pin 11: 2A1     (group 2 input)
//   Pin 12: 1Y4     (group 1 output)
//   Pin 13: 2A2     (group 2 input)
//   Pin 14: 1Y3     (group 1 output)
//   Pin 15: 2A3     (group 2 input)
//   Pin 16: 1Y2     (group 1 output)
//   Pin 17: 2A4     (group 2 input)
//   Pin 18: 1Y1     (group 1 output)
//   Pin 19: ~2G     (group 2 enable, active low)
//   Pin 20: VCC
//
// Behavior:  Y = A when ~G Low,  Y = HiZ when ~G High.
//   TTL: floating/HiZ input treated as High -> output High.
//   Buffer formula: level | (level - 1)
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74S244 : public CallbackComponent {
public:
    IC_74S244() : CallbackComponent("74S244") { set_description("Octal Buffer"); }

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

        // Enable pins
        pin_g1_ = connect_pin(1);   // ~1G (group 1 enable, active low)
        pin_g2_ = connect_pin(19);  // ~2G (group 2 enable, active low)

        // Group 1: inputs (2,4,6,8) -> outputs (18,16,14,12)
        grp1_[0].a = connect_pin(2);  grp1_[0].y = pin(18);  // 1A1 -> 1Y1
        grp1_[1].a = connect_pin(4);  grp1_[1].y = pin(16);  // 1A2 -> 1Y2
        grp1_[2].a = connect_pin(6);  grp1_[2].y = pin(14);  // 1A3 -> 1Y3
        grp1_[3].a = connect_pin(8);  grp1_[3].y = pin(12);  // 1A4 -> 1Y4

        // Group 2: inputs (11,13,15,17) -> outputs (9,7,5,3)
        grp2_[0].a = connect_pin(11); grp2_[0].y = pin(9);   // 2A1 -> 2Y1
        grp2_[1].a = connect_pin(13); grp2_[1].y = pin(7);   // 2A2 -> 2Y2
        grp2_[2].a = connect_pin(15); grp2_[2].y = pin(5);   // 2A3 -> 2Y3
        grp2_[3].a = connect_pin(17); grp2_[3].y = pin(3);   // 2A4 -> 2Y4

        // VCC
        Signal* vcc = socket.pin_signal(20);
        if (vcc) vcc->connect(this);

        // ~G1 and ~G2 feed bidir lambdas (sampled at permutation time), async.
        declare_input(pin_g1_);
        declare_input(pin_g2_);
        for (auto& b : grp1_) { declare_input(b.a); declare_input(b.y); declare_output(b.y); }
        for (auto& b : grp2_) { declare_input(b.a); declare_input(b.y); declare_output(b.y); }
    }

protected:
    void on_power_on() override { }
    void on_power_off() override { }

    void on_cycle(Fiber /*caller*/) override {
        if (pin_g1_.level() == Level::Low) {
            int8_t v;
            v = (int8_t)grp1_[0].a.level(); grp1_[0].y.drive((Level)(v | (v - 1)));
            v = (int8_t)grp1_[1].a.level(); grp1_[1].y.drive((Level)(v | (v - 1)));
            v = (int8_t)grp1_[2].a.level(); grp1_[2].y.drive((Level)(v | (v - 1)));
            v = (int8_t)grp1_[3].a.level(); grp1_[3].y.drive((Level)(v | (v - 1)));
        }
        if (pin_g2_.level() == Level::Low) {
            int8_t v;
            v = (int8_t)grp2_[0].a.level(); grp2_[0].y.drive((Level)(v | (v - 1)));
            v = (int8_t)grp2_[1].a.level(); grp2_[1].y.drive((Level)(v | (v - 1)));
            v = (int8_t)grp2_[2].a.level(); grp2_[2].y.drive((Level)(v | (v - 1)));
            v = (int8_t)grp2_[3].a.level(); grp2_[3].y.drive((Level)(v | (v - 1)));
        }
    }

private:

    struct Buffer {
        Pin a, y;    // input, output
    };
    Buffer grp1_[4];  // group 1: pins 2->18, 4->16, 6->14, 8->12
    Buffer grp2_[4];  // group 2: pins 11->9, 13->7, 15->5, 17->3
    Pin pin_g1_;      // ~1G (pin 1)
    Pin pin_g2_;      // ~2G (pin 19)
};

} // namespace bench
