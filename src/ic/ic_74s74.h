#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S74 Dual D Flip-Flop with Preset and Clear (Schottky).
//
// 14-pin DIP. U67, U82, U96 on the 5150.
//
// Pinout:
//   Pin  1: ~CLR1    (async clear FF1, active low)
//   Pin  2: D1       (data input FF1)
//   Pin  3: CLK1     (clock input FF1, positive-edge triggered)
//   Pin  4: ~PRE1    (async preset FF1, active low)
//   Pin  5: Q1       (output FF1)
//   Pin  6: ~Q1      (complementary output FF1)
//   Pin  7: GND
//   Pin  8: ~Q2      (complementary output FF2)
//   Pin  9: Q2       (output FF2)
//   Pin 10: ~PRE2    (async preset FF2, active low)
//   Pin 11: CLK2     (clock input FF2, positive-edge triggered)
//   Pin 12: D2       (data input FF2)
//   Pin 13: ~CLR2    (async clear FF2, active low)
//   Pin 14: VCC
//
// Behavior (per flip-flop):
//   ~CLR=L, ~PRE=H -> Q=L, ~Q=H  (clear)
//   ~CLR=H, ~PRE=L -> Q=H, ~Q=L  (preset)
//   ~CLR=L, ~PRE=L -> Q=H, ~Q=H  (both asserted, indeterminate)
//   ~CLR=H, ~PRE=H -> rising CLK edge latches D
//
// Threading: CallbackComponent -- sequential, no fiber.
class IC_74S74 : public CallbackComponent {
public:
    // async_clk: when true, CLK is declared async_input (breaks DAG cycles
    // for registered feedback paths like U82's N-000240 -> CLK -> N-000237).
    explicit IC_74S74(bool async_clk = false);

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_cycle(Fiber caller) override;

private:
    void update_ff(int i);
    void drive_ff(int i);

    struct FF {
        Pin clr, d, clk, pre, q, nq;
        bool q_state = false;
        Level clk_prev = Level::HiZ;
    };
    FF ff_[2];
    bool async_clk_ = false;
};

} // namespace bench
