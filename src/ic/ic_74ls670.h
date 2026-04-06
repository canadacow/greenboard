#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74LS670 4x4 Register File (Low-Power Schottky).
//
// 16-pin DIP. U19 on the 5150 (DMA page register).
//
// Pinout:
//   Pin  1: D1       (data input 1)
//   Pin  2: D2       (data input 2)
//   Pin  3: D3       (data input 3)
//   Pin  4: RA       (read address A -- LSB)
//   Pin  5: RB       (read address B -- MSB)
//   Pin  6: Q3       (data output 3)
//   Pin  7: Q2       (data output 2)
//   Pin  8: GND
//   Pin  9: Q1       (data output 1)
//   Pin 10: Q0       (data output 0)
//   Pin 11: ~RE      (read enable, active low -- tri-state control)
//   Pin 12: ~WE      (write enable, active low)
//   Pin 13: WB       (write address B -- MSB)
//   Pin 14: WA       (write address A -- LSB)
//   Pin 15: D0       (data input 0)
//   Pin 16: VCC
//
// Behavior:
//   Write: when ~WE=Low, data inputs D0-D3 are written to the register
//          selected by WA/WB. Writes are transparent (asynchronous).
//   Read:  outputs Q0-Q3 reflect the register selected by RA/RB.
//          Outputs are tri-stated when ~RE=High.
//   Simultaneous read/write to same address: outputs reflect input data.
//
// Threading: CallbackComponent -- combinational, no fiber.
class IC_74LS670 : public CallbackComponent {
public:
    IC_74LS670();

    /// Mark D inputs as async (cross-cycle) for DAG ordering.
    /// Use when writes are gated by a pulsed ~WE so the D->Q path
    /// is effectively cross-cycle, not combinational.
    void set_async_inputs() { async_d_ = true; }
    uint8_t reg(int i) const { return regs_[i & 3]; }

    // DMA output override: when true, bidir lambda returns Output
    // regardless of ~RE pin level. Set by 8237A one cycle ahead.
    void set_dma_output(bool en) { dma_output_ = en; }
    bool dma_output() const { return dma_output_; }

    void install(Socket& socket);

    void save(cereal::BinaryOutputArchive& ar) override { serialize(ar); }
    void load(cereal::BinaryInputArchive& ar) override { serialize(ar); }
    template <class Archive> void serialize(Archive& ar) {
        ar(regs_, driving_, dma_output_);
    }

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_cycle(Fiber caller) override;

private:
    void update();

    Pin pin_d_[4];     // D0-D3: pins 15, 1, 2, 3
    Pin pin_q_[4];     // Q0-Q3: pins 10, 9, 7, 6
    Pin pin_ra_, pin_rb_;   // Read address: pins 4, 5
    Pin pin_wa_, pin_wb_;   // Write address: pins 14, 13
    Pin pin_re_;       // ~RE: pin 11
    Pin pin_we_;       // ~WE: pin 12

    uint8_t regs_[4] = {};  // 4 x 4-bit registers
    bool async_d_ = false;
    bool driving_ = false;  // true when Q pins are actively driven (~RE=Low)
    bool dma_output_ = false;
};

} // namespace bench
