#pragma once
#include "core/callback_component.h"
#include "core/signal.h"
#include <cstdint>

namespace bench {

// SW2 multiplexer -- models U63 gate 3 (74S38 OC NAND) + U80 buffer 1 (74S125).
//
// PB2 selects which SW2 positions are visible on PPI Port C lower nibble:
//   PB2=High: N-000382 pulled Low by U63 -> positions 1-4 drive PC0-PC3
//   PB2=Low:  U80 enabled -> GND on N-000358 -> position 5 drives PC0
//             Positions 1-4 sense line floats High -> PC1-PC3 = High
//
// Replaces the IC_DipSwitch for SW2.  Switch values are static config.
class SW2Mux : public CallbackComponent {
public:
    SW2Mux() : CallbackComponent("SW2_Mux") {
        set_description("SW2 PB2 mux (U63+U80)");
    }

    // Wire: pb2 = input, pc[0..3] = outputs.
    void connect(Signal& pb2, Signal& pc0, Signal& pc1, Signal& pc2, Signal& pc3) {
        pb2.connect(this);
        pb2_pin_ = pb2.pin();
        declare_input(pb2_pin_);

        pc_[0] = &pc0; pc_[1] = &pc1; pc_[2] = &pc2; pc_[3] = &pc3;
        for (int i = 0; i < 4; ++i) {
            pc_[i]->connect(this);
            pc_pin_[i] = pc_[i]->pin();
            declare_output(pc_pin_[i]);
        }
    }

    // Set SW2 value. bit=1 means OFF (High/open), bit=0 means ON (Low/closed).
    // Bits 0-3 = positions 1-4, bit 4 = position 5, bits 5-7 = positions 6-8 (NC).
    void set_value(uint8_t val) { sw2_ = val; }
    uint8_t value() const { return sw2_; }

protected:
    void on_power_on() override { drive(); }
    void on_power_off() override {
        for (int i = 0; i < 4; ++i)
            pc_pin_[i].drive(Level::HiZ);
    }

    void on_cycle(Fiber /*caller*/) override {
        Level cur = pb2_pin_.level();
        if (cur == last_pb2_) return;
        last_pb2_ = cur;
        drive();
    }

private:
    void drive() {
        bool pb2_high = (pb2_pin_.level() == Level::High);
        if (pb2_high) {
            // Sense line active: positions 1-4 -> PC0-PC3
            // ON (bit=0) -> Low, OFF (bit=1) -> High
            for (int i = 0; i < 4; ++i)
                pc_pin_[i].drive((sw2_ & (1 << i)) ? Level::High : Level::Low);
        } else {
            // U80 enabled: position 5 -> PC0, PC1-PC3 = High (pull-up)
            pc_pin_[0].drive((sw2_ & 0x10) ? Level::High : Level::Low);
            for (int i = 1; i < 4; ++i)
                pc_pin_[i].drive(Level::High);
        }
    }

    Pin pb2_pin_{};
    Signal* pc_[4]{};
    Pin pc_pin_[4]{};
    Level last_pb2_ = Level::HiZ;
    uint8_t sw2_ = 0xFF;  // all OFF by default
};

} // namespace bench
