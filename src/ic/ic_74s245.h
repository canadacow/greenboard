#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// 74S245 Octal Bus Transceiver.
//
// 20-pin DIP. U8 on the 5150 (data bus transceiver).
//
// Standard pinout:
//   Pin  1: ~G (gate/enable, active low)
//   Pin  2: A1    Pin 18: B1
//   Pin  3: A2    Pin 17: B2
//   Pin  4: A3    Pin 16: B3
//   Pin  5: A4    Pin 15: B4
//   Pin  6: A5    Pin 14: B5
//   Pin  7: A6    Pin 13: B6
//   Pin  8: A7    Pin 12: B7
//   Pin  9: A8    Pin 11: B8
//   Pin 10: GND
//   Pin 19: DIR (direction: High=A->B, Low=B->A)
//   Pin 20: VCC
//
// 5150 wiring (U8):
//   A side = CPU local bus (AD0-AD7)
//   B side = system data bus (D0-D7)
//   ~G = ~DEN from 8288
//   DIR = DT/~R from 8288 (via gating logic)
//
// Threading: InlineComponent -- combinational, no thread.
class IC_74S245 : public CallbackComponent {
public:
    /// @param async_controls  When true, ~G and DIR are async inputs
    ///   (breaks DAG cycles for ISA bus buffers U13, U14).
    ///   CPU-local buffers (U8, U12) must use false for proper ordering.
    explicit IC_74S245(bool async_controls = false);

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_signal_change(Fiber caller) override;

private:
    void update_outputs();
    void release_outputs();

    Pin a_[8];   // A1=pin2 .. A8=pin9
    Pin b_[8];   // B1=pin18 .. B8=pin11

    // Track which side we're currently driving (only release what we drove)
    enum class Driving { None, A, B } driving_ = Driving::None;
    Pin g_;    // Pin  1: ~G (enable)
    Pin dir_;  // Pin 19: DIR
    bool async_controls_ = false;
};

} // namespace bench
