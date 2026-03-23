#pragma once
#include "core/callback_component.h"
#include "core/signal.h"
#include <cstdint>
#include <string>

namespace bench {

// DIP switch block (e.g. SW1: 8 positions on the 5150).
// Each switch position drives a signal Low (closed/ON) or High (open/OFF, pull-up).
// Participates in the DAG as a CallbackComponent -- drives every cycle.
class IC_DipSwitch : public CallbackComponent {
public:
    IC_DipSwitch(const char* name, int positions);

    // Wire switch position (0-based) to a signal. Declares output.
    void connect_position(int pos, Signal& signal);

    // Set a switch: true = ON (closed, Low), false = OFF (open, pulled High).
    void set(int pos, bool on);
    bool get(int pos) const;

    // Configure from SW1Config-style value byte (bit=1 means OFF/High).
    void set_value(uint8_t val);
    uint8_t value() const;

protected:
    void on_signal_change(Fiber caller) override;
    void on_power_on() override;
    void on_power_off() override;

private:
    void drive_all();

    struct Position {
        Pin pin{};
        bool on = false;   // true = closed = Low
    };
    Position pos_[8];
    int count_;
};

} // namespace bench
