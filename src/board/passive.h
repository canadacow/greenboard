#pragma once
#include "core/signal.h"
#include <string>
#include <vector>

namespace bench {

// A discrete resistor on the motherboard.
// Two-terminal passive: connects signal_a to signal_b.
// If one pin is on a power rail (VCC or GND), the resistor acts as a
// pull-up or pull-down on the other pin. The value is recorded for documentation.
struct Resistor {
    std::string ref;        // e.g. "R1"
    std::string value;      // e.g. "18K", "27", "510"
    Signal* signal_a = nullptr;
    Signal* signal_b = nullptr;

    // Derive pull behavior from wiring.
    // If one pin is VCC, pull the other High. If GND, pull it Low.
    void apply(Signal* vcc, Signal* gnd) {
        if (!signal_a || !signal_b) return;
        if (signal_a == vcc)       signal_b->set_pull(Level::High);
        else if (signal_b == vcc)  signal_a->set_pull(Level::High);
        else if (signal_a == gnd)  signal_b->set_pull(Level::Low);
        else if (signal_b == gnd)  signal_a->set_pull(Level::Low);
    }
};

// A resistor network (SIP/DIP package with multiple resistors).
// One pin is the common rail (typically VCC or GND). On 16-pin DIP
// packages this is pin 16 (top of package), not pin 1.
// The common pin is identified at apply() time by checking which pin
// is on a power rail, then pulling all other signal pins accordingly.
struct ResistorNetwork {
    std::string ref;        // e.g. "RN1"
    std::string value;      // e.g. "4.7K"
    int pin_count = 0;
    std::vector<Signal*> pins;          // All pins (0-indexed: pins[0] = pin 1)

    // Derive pull behavior from wiring.
    // Find the common pin (whichever pin is VCC or GND), then pull
    // all other signal pins accordingly.
    void apply(Signal* vcc, Signal* gnd) {
        Level pull = Level::HiZ;
        for (auto* pin : pins) {
            if (pin == vcc)  { pull = Level::High; break; }
            if (pin == gnd)  { pull = Level::Low;  break; }
        }
        if (pull == Level::HiZ) return;
        for (auto* pin : pins) {
            if (pin && pin != vcc && pin != gnd)
                pin->set_pull(pull);
        }
    }
};

// A discrete capacitor on the motherboard.
// Bypass caps connect VCC to GND; coupling caps connect two signals.
struct Capacitor {
    std::string ref;        // e.g. "C9"
    std::string value;      // e.g. ".01uF", ".047uF"
    Signal* signal_a = nullptr;
    Signal* signal_b = nullptr;
};

// A crystal oscillator.
struct Crystal {
    std::string ref;        // e.g. "Y1"
    std::string value;      // e.g. "14.31818MHz"
    Signal* osc_in = nullptr;
    Signal* osc_out = nullptr;
};

// A discrete diode.
struct Diode {
    std::string ref;        // e.g. "D1"
    std::string value;      // e.g. "TYPE_FC"
    Signal* anode = nullptr;
    Signal* cathode = nullptr;
};

// A relay (DPDT in the 5150 -- cassette motor control).
struct Relay {
    std::string ref;        // e.g. "K1"
    std::string value;      // e.g. "G5V-2"
    Signal* coil_a = nullptr;
    Signal* coil_b = nullptr;
    // Contact signals can be added as needed
};

// A trimmer capacitor / variable capacitor.
struct Trimmer {
    std::string ref;        // e.g. "VC1"
    std::string value;      // e.g. "5-30pF"
    Signal* signal_a = nullptr;
    Signal* signal_b = nullptr;
};

// A generic board connector (DIN, power, header pins, etc.).
// For ISA slots, use the dedicated IsaSlot class instead.
struct Connector {
    std::string ref;        // e.g. "J6", "P1"
    std::string value;      // e.g. "CASSETTE", "POWER_CON"
    int pin_count = 0;
    std::vector<Signal*> pins;
};

// A single jumper on the motherboard. When closed, drives a signal
// to a fixed level. When open, the signal floats (pull resistor decides).
struct Jumper {
    std::string ref;    // e.g. "E2"
    Signal* signal = nullptr;
    bool closed = false;

    void apply() {
        if (!signal) return;
        if (closed)
            signal->drive(Level::Low);   // jumper shorts to ground
        else
            signal->release();
    }
};

// DIP switch block (e.g. SW1: 8 positions on the 5150).
// Each switch position drives a signal Low (closed/ON) or releases it (open/OFF).
// Open positions float to pull-up (High) via resistor packs on the real board.
class DipSwitch {
public:
    DipSwitch(std::string ref, int positions);

    const std::string& ref() const { return ref_; }
    int positions() const { return static_cast<int>(switches_.size()); }

    // Wire switch position (1-based) to a signal.
    void wire(int position, Signal& signal);

    // Set a switch: true = ON (closed, Low), false = OFF (open, pulled High).
    void set(int position, bool on);
    bool get(int position) const;

    // Apply all switch settings to wired signals.
    void apply();

private:
    std::string ref_;
    struct Switch {
        Signal* signal = nullptr;
        bool on = false;
    };
    std::vector<Switch> switches_;
};

} // namespace bench
