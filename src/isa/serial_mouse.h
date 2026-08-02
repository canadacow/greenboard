#pragma once
// Microsoft two-button serial mouse (the DE-9 mouse on its 9-to-25
// adapter, hanging off the SixPakPlus's COM1).
//
// Protocol (Microsoft "M" series, 1200 baud 7N1):
//   - The mouse is powered from the RTS/DTR lines. A driver toggles
//     RTS low->high to reset the mouse; the mouse identifies itself
//     by transmitting 'M' (0x4D). This is how MOUSE.COM detects it.
//   - Movement/button events are 3-byte packets of 7-bit characters:
//       byte 1: 0 1 LB RB Y7 Y6 X7 X6   (bit 6 always set -- sync bit)
//       byte 2: 0 0 X5 X4 X3 X2 X1 X0
//       byte 3: 0 0 Y5 Y4 Y3 Y2 Y1 Y0
//     dx/dy are signed 8-bit deltas, Y positive = down. A packet is
//     sent whenever movement accumulates or a button changes.
//
// The UART polls poll_rx() once per character time (~120 cps at 1200
// baud), which naturally paces packet delivery the way the real mouse
// paces its own transmission.
//
// Host input arrives via host_update() -- thread-safe (the renderer
// thread writes, the clock thread drains via poll_rx()).

#include "isa/ins8250.h"
#include <atomic>
#include <cstdint>

namespace bench {

class SerialMouse final : public SerialDevice {
public:
    // Host-side input: relative motion (host pixels, Y down) and
    // current button states. Thread-safe, callable from any thread.
    void host_update(int dx, int dy, bool left, bool right) {
        acc_dx_.fetch_add(dx, std::memory_order_relaxed);
        acc_dy_.fetch_add(dy, std::memory_order_relaxed);
        buttons_.store((uint8_t)((left ? 1 : 0) | (right ? 2 : 0)),
                       std::memory_order_relaxed);
    }

    // --- SerialDevice ---

    void on_modem(bool /*dtr*/, bool rts) override {
        // Powered from the control lines; RTS rising edge = reset,
        // identify with 'M'.
        if (rts && !powered_) {
            head_ = tail_ = 0;
            sent_buttons_ = 0;
            acc_dx_.store(0, std::memory_order_relaxed);
            acc_dy_.store(0, std::memory_order_relaxed);
            push(0x4D);  // 'M'
        }
        powered_ = rts;
    }

    bool poll_rx(uint8_t& out) override {
        if (!powered_)
            return false;

        // Refill with a movement/button packet when the queue drains.
        if (head_ == tail_) {
            uint8_t btn = buttons_.load(std::memory_order_relaxed);
            int dx = acc_dx_.load(std::memory_order_relaxed);
            int dy = acc_dy_.load(std::memory_order_relaxed);
            if (dx == 0 && dy == 0 && btn == sent_buttons_)
                return false;

            // Take up to one packet's worth of motion; leave the rest
            // accumulated for the next packet.
            int px = (dx > 127) ? 127 : (dx < -128) ? -128 : dx;
            int py = (dy > 127) ? 127 : (dy < -128) ? -128 : dy;
            acc_dx_.fetch_sub(px, std::memory_order_relaxed);
            acc_dy_.fetch_sub(py, std::memory_order_relaxed);
            sent_buttons_ = btn;

            uint8_t x = (uint8_t)(int8_t)px;
            uint8_t y = (uint8_t)(int8_t)py;
            push((uint8_t)(0x40 |
                           ((btn & 1) ? 0x20 : 0) |     // left
                           ((btn & 2) ? 0x10 : 0) |     // right
                           ((y >> 4) & 0x0C) |          // Y7-Y6
                           ((x >> 6) & 0x03)));         // X7-X6
            push((uint8_t)(x & 0x3F));
            push((uint8_t)(y & 0x3F));
        }

        out = q_[head_];
        head_ = (uint8_t)((head_ + 1) & (QLEN - 1));
        return true;
    }

    // The mouse steals its power from RTS/DTR; it has no drivers for
    // CTS/DSR/RI/DCD, so the modem inputs stay unasserted.
    uint8_t modem_in() override { return 0; }

    // Test/debug: is the mouse currently powered (RTS high)?
    bool powered() const { return powered_; }

    // Full power-cycle reset (test harness use: the UART's own reset()
    // doesn't imply an on_modem() callback, so mouse-side power state
    // must be cleared explicitly between test runs).
    void reset() {
        powered_ = false;
        head_ = tail_ = 0;
        sent_buttons_ = 0;
        acc_dx_.store(0, std::memory_order_relaxed);
        acc_dy_.store(0, std::memory_order_relaxed);
        buttons_.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr int QLEN = 8;
    void push(uint8_t b) {
        uint8_t next = (uint8_t)((tail_ + 1) & (QLEN - 1));
        if (next == head_) return;  // full: drop (real mice have no buffer at all)
        q_[tail_] = b;
        tail_ = next;
    }

    // Clock-thread state.
    bool powered_ = false;
    uint8_t q_[QLEN] = {};
    uint8_t head_ = 0, tail_ = 0;
    uint8_t sent_buttons_ = 0;

    // Host-thread input accumulators.
    std::atomic<int> acc_dx_{0};
    std::atomic<int> acc_dy_{0};
    std::atomic<uint8_t> buttons_{0};
};

} // namespace bench
