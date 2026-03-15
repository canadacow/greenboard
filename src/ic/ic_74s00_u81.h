#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// U81: 74S00 Quad NAND on the 5150 motherboard.
//
// Special case: RAS appears on both pin 2 (gate 1 input) and pin 6
// (gate 2 output). Gate 2 drives RAS from ~MEMR/~MEMW. Gate 1 reads
// RAS to produce ~REFRSH_GATE. These never happen in the same cycle.
//
// Tracks which direction RAS is this cycle so the scheduler can order
// TD1 (which reads RAS) correctly.
class IC_74S00_U81 : public CallbackComponent {
public:
    IC_74S00_U81();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change(Fiber caller) override;

private:
    struct Gate {
        Pin a, b, y;
    };
    Gate gates_[4];

    // RAS: gate 2 always drives, gate 1 always reads. Always Output.
    Pin ras_pin_;
};

} // namespace bench
