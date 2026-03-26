#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74LS02 Quad 2-Input NOR Gate.
//
// 14-pin DIP. U27 on the 5150, U50 on the 5150.
//
// Pinout (NOR: Y first for gates 1-2, A first for gates 3-4):
//   Pin  1: Y1       (gate 1 output)
//   Pin  2: A1       (gate 1 input)
//   Pin  3: B1       (gate 1 input)
//   Pin  4: Y2       (gate 2 output)
//   Pin  5: A2       (gate 2 input)
//   Pin  6: B2       (gate 2 input)
//   Pin  7: GND
//   Pin  8: A3       (gate 3 input)
//   Pin  9: B3       (gate 3 input)
//   Pin 10: Y3       (gate 3 output)
//   Pin 11: A4       (gate 4 input)
//   Pin 12: B4       (gate 4 input)
//   Pin 13: Y4       (gate 4 output)
//   Pin 14: VCC
//
// Behavior (per gate):
//   Y = ~(A | B)   (TTL: HiZ treated as Low input)
//   NOR formula: invert the max of both inputs: -(max(a,b)) | 1
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74LS02 : public CallbackComponent {
public:
    IC_74LS02() : CallbackComponent("74LS02") { set_description("Quad 2-In NOR"); }

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

        // NOR pinout: gates 1,2 have Y,A,B; gates 3,4 have A,B,Y
        gates_[0].a = connect_pin(2);  gates_[0].b = connect_pin(3);  gates_[0].y = pin(1);
        gates_[1].a = connect_pin(5);  gates_[1].b = connect_pin(6);  gates_[1].y = pin(4);
        gates_[2].a = connect_pin(8);  gates_[2].b = connect_pin(9);  gates_[2].y = pin(10);
        gates_[3].a = connect_pin(11); gates_[3].b = connect_pin(12); gates_[3].y = pin(13);

        Signal* vcc = socket.pin_signal(14);
        if (vcc) vcc->connect(this);

        for (auto& g : gates_) {
            declare_input(g.a);
            declare_input(g.b);
            declare_output(g.y);
        }
    }

protected:
    void on_power_on() override { eval_gates(); }
    void on_power_off() override {
        gates_[0].y.release(); gates_[1].y.release();
        gates_[2].y.release(); gates_[3].y.release();
    }

    void on_cycle(Fiber /*caller*/) override {
        // Two passes: gate outputs may feed other gates within the same IC
        // (e.g. U27: gate 4 output -> gate 1 input).
        eval_gates();
    }

private:
    static Level nor(Pin& a, Pin& b) {
        int8_t m = std::max((int8_t)a.level(), (int8_t)b.level());
        return (Level)(-m | 1);
    }

    void eval_gates() {
        gates_[0].y.drive(nor(gates_[0].a, gates_[0].b));
        gates_[1].y.drive(nor(gates_[1].a, gates_[1].b));
        gates_[2].y.drive(nor(gates_[2].a, gates_[2].b));
        gates_[3].y.drive(nor(gates_[3].a, gates_[3].b));
    }

    struct Gate { Pin a, b, y; };
    Gate gates_[4];
};

} // namespace bench
