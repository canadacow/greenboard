#pragma once
#include "core/callback_component.h"
#include "core/signal.h"
#include <cstdint>

namespace bench {

// SW1 multiplexer -- drives PPI Port A with DIP switch values.
//
// Replaces U23 (74S244) + DIP switch IC + remap table with a single
// component that drives PA0-PA7 directly from the configured SW1 byte.
//
// PB7 controls the mux (like real hardware):
//   PB7=High (PBKB=clear KB): U23 enabled, switches drive PA
//   PB7=Low  (PBKB=enable KB): U23 disabled, keyboard drives PA
//
// When PB7=High, drives PA with SW1 equipment value.
// When PB7=Low, releases PA so the keyboard can drive it.
class SW1Mux : public CallbackComponent {
public:
    SW1Mux() : CallbackComponent("SW1_Mux") {
        set_description("SW1 PB7 mux (U23)");
    }

    // Wire: pb7 = select input, pa[0..7] = outputs.
    void connect(Signal& pb7, Signal* pa[8]) {
        pb7.connect(this);
        pb7_pin_ = pb7.pin();
        declare_input(pb7_pin_);

        for (int i = 0; i < 8; ++i) {
            pa[i]->connect(this);
            pa_pin_[i] = pa[i]->pin();
            declare_output(pa_pin_[i]);
        }
    }

    // Set SW1 equipment byte. Bits map 1:1 to PA0-PA7.
    // This is the value the BIOS will read from PPI Port A.
    void set_value(uint8_t val) { sw1_ = val; }
    uint8_t value() const { return sw1_; }

protected:
    void on_power_on() override { drive(); }
    void on_power_off() override {
        for (int i = 0; i < 8; ++i)
            pa_pin_[i].release();
    }

    void on_cycle(Fiber /*caller*/) override {
        Level cur = pb7_pin_.level();
        if (cur == last_pb7_) return;
        last_pb7_ = cur;
        drive();
    }

private:
    void drive() {
        if (pb7_pin_.level() == Level::High) {
            // PB7=High: switches selected, drive PA with SW1
            for (int i = 0; i < 8; ++i)
                pa_pin_[i].drive((sw1_ & (1 << i)) ? Level::High : Level::Low);
        } else {
            // PB7=Low: keyboard selected, release PA
            for (int i = 0; i < 8; ++i)
                pa_pin_[i].release();
        }
    }

    Pin pb7_pin_{};
    Pin pa_pin_[8]{};
    Level last_pb7_ = Level::HiZ;
    uint8_t sw1_ = 0xFF;
};

} // namespace bench
