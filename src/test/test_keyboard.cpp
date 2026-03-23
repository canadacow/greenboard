#include "test_keyboard.h"
#include <spdlog/spdlog.h>

namespace bench {

const uint8_t TestKeyboard::ascii_to_make_[128] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,
    0x39,0x02,0x28,0x04,0x05,0x06,0x08,0x28,0x0A,0x0B,0x09,0x0D,0x33,0x0C,0x34,0x35,
    0x0B,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x27,0x27,0x33,0x0D,0x34,0x35,
    0x03,0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,
    0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C,0x1A,0x2B,0x1B,0x07,0x0C,
    0x29,0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,
    0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C,0x1A,0x2B,0x1B,0x29,0x00,
};

const bool TestKeyboard::ascii_needs_shift_[128] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,1,1,1,1,1,1,0,1,1,1,1,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,1,0,1,0,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,
};

TestKeyboard::TestKeyboard()
    : CallbackComponent("TestKBD") {
    set_description("Keyboard");
}

void TestKeyboard::connect(Signal* pa[8], Signal& irq1, Signal& pb6,
                           Signal& pb7, Signal& ready, Signal& ack) {
    for (int i = 0; i < 8; ++i) {
        pin_pa_[i] = pa[i]->pin();
        declare_output(pin_pa_[i]);
    }
    pin_irq1_ = irq1.pin();
    declare_output(pin_irq1_);

    pb6.connect(this);
    pin_pb6_ = pb6.pin();
    declare_input(pin_pb6_);

    pb7.connect(this);
    pin_pb7_ = pb7.pin();
    declare_input(pin_pb7_);

    ready.connect(this);
    pin_ready_ = ready.pin();
    declare_input(pin_ready_);

    ack.connect(this);
    pin_ack_ = ack.pin();
    declare_input(pin_ack_);
}

void TestKeyboard::on_power_on() {
    queue_.clear();
    queue_pos_ = 0;
    armed_ = false;
    // Don't drive PA pins on power-on -- leave them for DIP switches.
    // We only start driving PA (Low = idle) after being armed.
    waiting_ack_ = false;
    deliver_pending_ = false;
    reset_pending_ = false;
    reset_delay_ = 0;
    ready_prev_ = Level::HiZ;
    ack_prev_ = Level::HiZ;
    pb6_prev_ = Level::HiZ;
    pb7_prev_ = Level::HiZ;
    pa_driven_ = 0;
    pin_irq1_.drive(Level::Low);
}

void TestKeyboard::enqueue(uint8_t scancode) {
    queue_.push_back(scancode);
}

void TestKeyboard::enqueue_string(const char* text) {
    for (const char* p = text; *p; ++p) {
        uint8_t ch = static_cast<uint8_t>(*p);
        if (ch >= 128) continue;
        uint8_t make = ascii_to_make_[ch];
        if (!make) continue;
        bool shift = ascii_needs_shift_[ch];
        if (shift) enqueue(0x2A);
        enqueue(make);
        enqueue(make | 0x80);
        if (shift) enqueue(0xAA);
    }
}

void TestKeyboard::on_signal_change(Fiber /*caller*/) {
    Level ready_cur = pin_ready_.level();
    Level ack_cur = pin_ack_.level();
    Level pb6_cur = pin_pb6_.level();
    Level pb7_cur = pin_pb7_.level();

    // Detect keyboard reset protocol via PB6 (KBD CLK inhibit):
    // PB6 Low = CLK pulled low (reset start).
    // PB6 High after Low = CLK released (reset complete) -> send 0xAA.
    if (pb6_cur == Level::Low && pb6_prev_ != Level::Low) {
        reset_pending_ = true;
    }
    if (reset_pending_ && pb6_cur == Level::High && pb6_prev_ != Level::High) {
        reset_pending_ = false;
        // Insert 0xAA self-test response at current queue position.
        queue_.insert(queue_.begin() + static_cast<ptrdiff_t>(queue_pos_), 0xAA);
        armed_ = true;
        // Delay delivery so the CPU has time to unmask IRQ1 and STI.
        // Real keyboard takes ~20ms; we just need enough cycles for
        // the BIOS to execute the unmask + STI instructions.
        reset_delay_ = 200;
    }

    // Countdown for delayed reset delivery.
    if (reset_delay_ > 0) {
        if (--reset_delay_ == 0)
            deliver_pending_ = true;
    }

    // Wait for test program to arm us (non-reset path).
    if (!armed_) {
        if (ready_cur == Level::High && ready_prev_ != Level::High) {
            armed_ = true;
            deliver_next();
        }
        ready_prev_ = ready_cur;
        ack_prev_ = ack_cur;
        pb6_prev_ = pb6_cur;
        pb7_prev_ = pb7_cur;
        return;
    }

    // Deferred delivery: IRQ1 was lowered last cycle, now raise with new scancode.
    // This ensures the PIC sees a clean Low->High edge.
    if (deliver_pending_) {
        deliver_pending_ = false;
        deliver_next();
    }

    // PB7 High = U24 CLEAR: zero the shift register outputs.
    // This does NOT advance the queue -- it just clears PA like real hardware.
    if (pb7_cur == Level::High)
        pa_driven_ = 0;

    // ACK: port 0xFD write from IRQ handler.
    if (ack_cur == Level::High && ack_prev_ != Level::High && waiting_ack_) {
        handle_ack("0xFD");
        pin_ack_.drive(Level::Low);
    }

    // Re-drive PA every cycle (like real U24 shift register holds its outputs).
    for (int i = 0; i < 8; ++i)
        pin_pa_[i].drive((pa_driven_ >> i) & 1 ? Level::High : Level::Low);

    ready_prev_ = ready_cur;
    ack_prev_ = ack_cur;
    pb6_prev_ = pb6_cur;
    pb7_prev_ = pb7_cur;
}

void TestKeyboard::handle_ack(const char* source) {
    release_scancode();
    pin_irq1_.drive(Level::Low);
    waiting_ack_ = false;
    deliver_pending_ = true;  // deliver next cycle so PIC sees Low->High edge
}

void TestKeyboard::deliver_next() {
    if (queue_pos_ >= queue_.size()) return;
    uint8_t sc = queue_[queue_pos_++];
    drive_scancode(sc);
    pin_irq1_.drive(Level::High);
    waiting_ack_ = true;
}

void TestKeyboard::drive_scancode(uint8_t sc) {
    pa_driven_ = sc;
}

void TestKeyboard::release_scancode() {
    pa_driven_ = 0x00;
}

} // namespace bench
