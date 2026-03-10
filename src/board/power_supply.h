#pragma once
#include "core/signal.h"

namespace bench {

// IBM PC 5150 power supply unit.
// Provides +5V, -5V, +12V, -12V rails and a POWER_GOOD signal.
// Modeled as enable signals: when a rail is High, components on
// that rail are powered. POWER_GOOD goes High ~100-500ms after
// rails stabilize (triggers 8284 reset logic on real hardware).
struct PowerSupply {
    Signal power_good{"POWER_GOOD"};
    Signal vcc{"+5V"};
    Signal vcc_12{"+12V"};
    Signal vcc_n5{"-5V"};
    Signal vcc_n12{"-12V"};
    Signal gnd{"GND"};

    // Flip the switch on the back of the PC.
    void switch_on() {
        gnd.drive(Level::Low);
        vcc.drive(Level::High);
        vcc_12.drive(Level::High);
        vcc_n5.drive(Level::High);
        vcc_n12.drive(Level::High);
        // Real PSU delays POWER_GOOD by ~100-500ms after rails stabilize.
        // For now, assert immediately. Timing can be added later.
        power_good.drive(Level::High);
    }

    void switch_off() {
        power_good.drive(Level::Low);
        vcc.drive(Level::HiZ);
        vcc_12.drive(Level::HiZ);
        vcc_n5.drive(Level::HiZ);
        vcc_n12.drive(Level::HiZ);
        gnd.drive(Level::HiZ);
    }
};

} // namespace bench
