#pragma once
#include "core/callback_component.h"
#include "core/signal.h"
#include <vector>
#include <cstdint>

namespace bench {

// Keyboard simulator for the IBM PC 5150 test bench.
//
// Bypasses U24 (74S322 shift register) -- drives PA0-PA7 directly
// with queued scancodes and asserts IRQ1 on the PIC.
//
// ACK via two independent paths (either triggers next scancode):
//   1. PB7 (real hardware): PPI Port B bit 7 toggle from IRQ handler
//   2. kbd_ack (test failsafe): testcard port 0xFD write from IRQ handler
//
// Armed by kbd_ready signal (testcard port 0xFC write).
class TestKeyboard : public CallbackComponent {
public:
    TestKeyboard();

    void connect(Signal* pa[8], Signal& irq1, Signal& pb6, Signal& pb7,
                 Signal& ready, Signal& ack);

    void enqueue(uint8_t scancode);
    void enqueue_string(const char* text);

protected:
    void on_cycle(Fiber caller) override;
    void on_power_on() override;

private:
    void drive_scancode(uint8_t sc);
    void release_scancode();
    void deliver_next();
    void handle_ack(const char* source);

    Pin pin_pa_[8]{};
    Pin pin_irq1_{};
    Pin pin_pb6_{};       // KBD CLK inhibit (PPI Port B bit 6)
    Pin pin_pb7_{};       // real hardware ACK (PPI Port B bit 7)
    Pin pin_ready_{};     // testcard port 0xFC
    Pin pin_ack_{};       // testcard port 0xFD (failsafe ACK)

    std::vector<uint8_t> queue_;
    size_t queue_pos_ = 0;
    bool armed_ = false;
    bool waiting_ack_ = false;
    bool deliver_pending_ = false;  // deliver next scancode on next cycle
    bool reset_pending_ = false;    // PB6 went Low -- waiting for release
    int  reset_delay_ = 0;          // cycles to wait before delivering 0xAA
    Level ready_prev_ = Level::HiZ;
    Level ack_prev_ = Level::HiZ;
    Level pb6_prev_ = Level::HiZ;
    Level pb7_prev_ = Level::HiZ;
    uint8_t pa_driven_ = 0;           // current PA value (re-driven every cycle like U24)

    static const uint8_t ascii_to_make_[128];
    static const bool    ascii_needs_shift_[128];
};

} // namespace bench
