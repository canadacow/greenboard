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
    pb6_prev_ = Level::Low;  // PPI resets Port B to 0; match that so init OUT doesn't false-trigger
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

void TestKeyboard::on_cycle(Fiber /*caller*/) {
    // Drain injected keys from UI thread into the queue.
    {
        int t = inject_tail_.load(std::memory_order_relaxed);
        int h = inject_head_.load(std::memory_order_acquire);
        bool had_keys = (t != h);
        while (t != h) {
            enqueue(inject_ring_[t]);
            t = (t + 1) % INJECT_RING;
        }
        inject_tail_.store(t, std::memory_order_release);
        // Live keys auto-arm the keyboard and trigger immediate delivery.
        // Clear waiting_ack if stuck -- the BIOS may have skipped the ack
        // (e.g. 301 error path) and we need to keep delivering.
        // Drive IRQ1 Low this cycle; deliver_pending_ fires NEXT cycle
        // so the PIC sees a clean Low->High edge.
        if (had_keys) {
            if (waiting_ack_) {
                waiting_ack_ = false;
            }
            pin_irq1_.drive(Level::Low);  // Low this cycle
            armed_ = true;
            deliver_pending_ = true;      // High next cycle (deliver_next)
            // Skip deliver_pending_ processing below -- must wait one cycle
            ready_prev_ = pin_ready_.level();
            ack_prev_ = pin_ack_.level();
            pb6_prev_ = pin_pb6_.level();
            pb7_prev_ = pin_pb7_.level();
            return;
        }
    }

    Level ready_cur = pin_ready_.level();
    Level ack_cur = pin_ack_.level();
    Level pb6_cur = pin_pb6_.level();
    Level pb7_cur = pin_pb7_.level();

    // Detect keyboard reset protocol via PB6 (KBD CLK inhibit):
    // PB6 Low = CLK pulled low (reset start).
    // PB6 High after Low = CLK released (reset complete) -> send 0xAA.
    if (pb6_cur == Level::Low && pb6_prev_ == Level::High) {
        reset_pending_ = true;
    }
    if (reset_pending_ && pb6_cur == Level::High && pb6_prev_ != Level::High) {
        reset_pending_ = false;
        queue_.insert(queue_.begin() + static_cast<ptrdiff_t>(queue_pos_), 0xAA);
        armed_ = true;
        reset_delay_ = 100;
    }

    // Countdown for delayed reset delivery.
    if (reset_delay_ > 0) {
        if (--reset_delay_ == 0) {
            deliver_pending_ = true;
            armed_ = true;
        }
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
    if (queue_pos_ >= queue_.size()) {
        return;
    }
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

void TestKeyboard::inject_key(uint8_t scancode) {
    int h = inject_head_.load(std::memory_order_relaxed);
    int next = (h + 1) % INJECT_RING;
    if (next == inject_tail_.load(std::memory_order_acquire)) return; // full
    inject_ring_[h] = scancode;
    inject_head_.store(next, std::memory_order_release);
}

// VK_ (Windows virtual key) -> XT make code. 0 = unmapped.
uint8_t TestKeyboard::vk_to_xt(int vk) {
    static uint8_t map[256] = {};
    static bool init = false;
    if (!init) {
        init = true;
        // 0x08=BS, 0x09=TAB, 0x0D=ENTER
        map[0x08]=0x0E; map[0x09]=0x0F; map[0x0D]=0x1C;
        // 0x10=SHIFT, 0x11=CTRL, 0x12=ALT, 0x14=CAPS, 0x1B=ESC
        map[0x10]=0x2A; map[0x11]=0x1D; map[0x12]=0x38; map[0x14]=0x3A; map[0x1B]=0x01;
        // 0x20=SPACE, 0x21-0x28=PGUP..DOWN arrows
        map[0x20]=0x39; map[0x21]=0x49; map[0x22]=0x51; map[0x23]=0x4F;
        map[0x24]=0x47; map[0x25]=0x4B; map[0x26]=0x48; map[0x27]=0x4D; map[0x28]=0x50;
        map[0x2D]=0x52; map[0x2E]=0x53; // INS, DEL
        // 0-9
        map[0x30]=0x0B; map[0x31]=0x02; map[0x32]=0x03; map[0x33]=0x04; map[0x34]=0x05;
        map[0x35]=0x06; map[0x36]=0x07; map[0x37]=0x08; map[0x38]=0x09; map[0x39]=0x0A;
        // A-Z
        map[0x41]=0x1E; map[0x42]=0x30; map[0x43]=0x2E; map[0x44]=0x20; map[0x45]=0x12;
        map[0x46]=0x21; map[0x47]=0x22; map[0x48]=0x23; map[0x49]=0x17; map[0x4A]=0x24;
        map[0x4B]=0x25; map[0x4C]=0x26; map[0x4D]=0x32; map[0x4E]=0x31; map[0x4F]=0x18;
        map[0x50]=0x19; map[0x51]=0x10; map[0x52]=0x13; map[0x53]=0x1F; map[0x54]=0x14;
        map[0x55]=0x16; map[0x56]=0x2F; map[0x57]=0x11; map[0x58]=0x2D; map[0x59]=0x15;
        map[0x5A]=0x2C;
        // Numpad
        map[0x60]=0x52; map[0x61]=0x4F; map[0x62]=0x50; map[0x63]=0x51; map[0x64]=0x4B;
        map[0x65]=0x4C; map[0x66]=0x4D; map[0x67]=0x47; map[0x68]=0x48; map[0x69]=0x49;
        map[0x6A]=0x37; map[0x6B]=0x4E; map[0x6D]=0x4A; map[0x6E]=0x53;
        // F1-F10
        map[0x70]=0x3B; map[0x71]=0x3C; map[0x72]=0x3D; map[0x73]=0x3E; map[0x74]=0x3F;
        map[0x75]=0x40; map[0x76]=0x41; map[0x77]=0x42; map[0x78]=0x43; map[0x79]=0x44;
        // NUMLOCK, SCROLLLOCK
        map[0x90]=0x45; map[0x91]=0x46;
        // OEM keys
        map[0xBA]=0x27; // ;
        map[0xBB]=0x0D; // =
        map[0xBC]=0x33; // ,
        map[0xBD]=0x0C; // -
        map[0xBE]=0x34; // .
        map[0xBF]=0x35; // /
        map[0xC0]=0x29; // `
        map[0xDB]=0x1A; // [
        map[0xDC]=0x2B; // backslash
        map[0xDD]=0x1B; // ]
        map[0xDE]=0x28; // '
    }
    return (vk >= 0 && vk < 256) ? map[vk] : 0;
}

} // namespace bench
