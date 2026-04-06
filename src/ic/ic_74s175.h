#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S175 Quad D Flip-Flop (Schottky).
//
// 16-pin DIP. Four positive-edge-triggered D flip-flops with
// complementary outputs and asynchronous master clear.
//
// Pin functions:
//   Pin  1: ~CLR  (input, master clear -- active low, async)
//   Pin  2: 1Q    (output)
//   Pin  3: ~1Q   (output, complement)
//   Pin  4: 1D    (input)
//   Pin  5: 2D    (input)
//   Pin  6: ~2Q   (output, complement)
//   Pin  7: 2Q    (output)
//   Pin  8: GND
//   Pin  9: CLK   (input, clock -- rising edge triggered)
//   Pin 10: Q2    (output)
//   Pin 11: ~Q2   (output, complement)
//   Pin 12: D2    (input)
//   Pin 13: D3    (input)
//   Pin 14: ~Q3   (output, complement)
//   Pin 15: Q3    (output)
//   Pin 16: VCC
//
// U26 on the 5150:
//   FF0+FF1: 2-stage keyboard data synchronizer (KBD_DATA -> 1D -> 1Q=2D -> ~2Q)
//   FF2: PCLK / 2 divider (~3Q fed back to 3D, 3Q = 1.193 MHz PIT clock)
//   FF3: unused
//
// Threading: Inline IC. Executes synchronously in the driving thread.
// CLK rising edge latches D inputs, ~CLR async clears all outputs.
class IC_74S175 : public CallbackComponent {
public:
    IC_74S175() : CallbackComponent("74S175") { set_description("Quad D Flip-Flop"); }

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

        pin_clr_ = connect_pin(1);
        pin_clk_ = connect_pin(9);
        pin_vcc_ = connect_pin(16);

        pin_d_[0] = connect_pin(4);
        pin_d_[1] = connect_pin(5);
        pin_d_[2] = connect_pin(12);
        pin_d_[3] = connect_pin(13);

        pin_q_[0] = pin(2);
        pin_q_[1] = pin(7);
        pin_q_[2] = pin(10);
        pin_q_[3] = pin(15);

        pin_nq_[0] = pin(3);
        pin_nq_[1] = pin(6);
        pin_nq_[2] = pin(11);
        pin_nq_[3] = pin(14);

        declare_input(pin_clr_); declare_input(pin_clk_);
        for (int i = 0; i < 4; ++i) declare_async_input(pin_d_[i]);
        for (int i = 0; i < 4; ++i) { declare_output(pin_q_[i]); declare_output(pin_nq_[i]); }
    }

    // Override a specific D pin from async to sync (adds DAG edge).
    void set_d_sync(int index) { declare_input(pin_d_[index]); }

    void save(cereal::BinaryOutputArchive& ar) override { serialize(ar); }
    void load(cereal::BinaryInputArchive& ar) override { serialize(ar); }
    template <class Archive> void serialize(Archive& ar) {
        ar(clr_prev_);
    }

protected:
    void on_power_on() override {
        clr_prev_ = Level::HiZ;
        drive_clear();
    }

    void on_power_off() override {
        pin_q_[0].release(); pin_nq_[0].release();
        pin_q_[1].release(); pin_nq_[1].release();
        pin_q_[2].release(); pin_nq_[2].release();
        pin_q_[3].release(); pin_nq_[3].release();
    }

    void on_cycle(Fiber /*caller*/) override {
        Level clr = pin_clr_.level();

        // ~CLR falling edge: async clear
        if (clr == Level::Low && clr_prev_ != Level::Low)
            drive_clear();
        clr_prev_ = clr;

        // Latch D -> Q, ~Q every cycle (when not cleared)
        if (clr != Level::Low) {
            int8_t v;
            v = (int8_t)pin_d_[0].level(); pin_q_[0].drive((Level)(v | (v - 1))); pin_nq_[0].drive((Level)((-v) | 1));
            v = (int8_t)pin_d_[1].level(); pin_q_[1].drive((Level)(v | (v - 1))); pin_nq_[1].drive((Level)((-v) | 1));
            v = (int8_t)pin_d_[2].level(); pin_q_[2].drive((Level)(v | (v - 1))); pin_nq_[2].drive((Level)((-v) | 1));
            v = (int8_t)pin_d_[3].level(); pin_q_[3].drive((Level)(v | (v - 1))); pin_nq_[3].drive((Level)((-v) | 1));
        }
    }

private:
    void drive_clear() {
        pin_q_[0].drive(Level::Low); pin_nq_[0].drive(Level::High);
        pin_q_[1].drive(Level::Low); pin_nq_[1].drive(Level::High);
        pin_q_[2].drive(Level::Low); pin_nq_[2].drive(Level::High);
        pin_q_[3].drive(Level::Low); pin_nq_[3].drive(Level::High);
    }

    Pin pin_clr_;
    Pin pin_clk_;
    Pin pin_vcc_;
    Pin pin_d_[4];
    Pin pin_q_[4];
    Pin pin_nq_[4];
    Level clr_prev_ = Level::HiZ;
};

} // namespace bench
