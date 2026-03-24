#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// U81: 74S00 Quad NAND on the 5150 motherboard.
//
// Special case: RAS appears on both pin 2 (gate 1 input) and pin 6
// (gate 2 output). Gate 2 drives RAS from ~MEMR/~MEMW. Gate 1 reads
// RAS to produce ~REFRSH_GATE.
//
// Gate 3 on real hardware reads TD1 (delay line) outputs -- delayed
// copies of RAS. TD1 is a ~100ns analog delay, sub-clock-cycle. The
// emulator can't model sub-eval delays with a separate component, so
// gate 3 uses an internal one-eval delay of RAS instead of reading
// its physical pin inputs.
class IC_74S00_U81 : public CallbackComponent {
public:
    IC_74S00_U81();

    void install(Socket& socket);

    // U81 internally produces delayed-RAS for gate 3 (~CAS).
    // ADDR_SEL is the same delayed-RAS signal routed to the 74S158 muxes.
    void connect_addr_sel(Signal& addr_sel);

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_cycle(Fiber caller) override;

private:
    struct Gate {
        Pin a, b, y;
    };
    Gate gates_[4];

    Pin ras_pin_;

    // Virtual TD1: previous RAS value used for gate 3 and ADDR_SEL
    Level td1_pending_ = Level::HiZ;
    Pin addr_sel_pin_;
};

} // namespace bench
