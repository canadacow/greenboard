#pragma once
#include "core/bus_controller_component.h"
#include "board/socket.h"

namespace bench {

// Intel 8288 Bus Controller.
//
// 20-pin DIP. Decodes 8088 status lines (S0-S2) into system bus
// control signals in maximum mode.
//
// Pin functions (from BRD/datasheet):
//   Pin  1: GND
//   Pin  2: CLK      (input, system clock from 8284A)
//   Pin  3: ~S1      (input, status from 8088)
//   Pin  4: ~DEN     (output, data enable -- controls 74S245 transceivers)
//   Pin  5: ALE      (output, address latch enable -- gates 74S373 latches)
//   Pin  6: CEN      (input, command enable -- from AEN_BRD)
//   Pin  7: ~MEMR    (output, memory read -- active low)
//   Pin  8: ~MEMW    (output, memory write -- active low)
//   Pin  9: NC
//   Pin 10: GND
//   Pin 11: NC
//   Pin 12: ~IOW     (output, I/O write -- active low)
//   Pin 13: ~IOR     (output, I/O read -- active low)
//   Pin 14: ~INTA    (output, interrupt acknowledge -- active low)
//   Pin 15: ~AEN     (input, address enable from DMA)
//   Pin 16: DT/~R    (output, data transmit/receive direction)
//   Pin 17: NC
//   Pin 18: ~S2      (input, status from 8088)
//   Pin 19: ~S0      (input, status from 8088)
//   Pin 20: VCC      (+5V)
//
// Bus cycle decoding (active low status):
//   ~S2 ~S1 ~S0 | Cycle
//    0   0   0  | INTA  (interrupt acknowledge)
//    0   0   1  | IOR   (I/O read)
//    0   1   0  | IOW   (I/O write)
//    0   1   1  | Halt
//    1   0   0  | Fetch (opcode fetch = memory read)
//    1   0   1  | MemR  (memory read)
//    1   1   0  | MemW  (memory write)
//    1   1   1  | Passive (no bus cycle)
//
// Timing (reactive to CLK rising edge):
//   T1: ALE asserted, DT/~R set for read/write direction
//   T2: ALE deasserted, command strobe active, DEN active
//   T3: Command stays active
//   T4: Command deasserted, DEN deasserted, back to idle
//
// Inline IC -- runs in the fixed-point loop so ALE/~DEN/commands are
// visible to other inlines (74S373 latches, 74S245 transceivers) in the
// same evaluation cycle, before fibers resume.
class IC_8288 : public BusControllerComponent {
public:
    IC_8288();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_signal_change(bool rising, bool falling) override;

private:
    // Bus cycle type decoded from S0-S2
    enum class BusCycle { Passive, INTA, IOR, IOW, Halt, Fetch, MemR, MemW };

    // T-state within a bus cycle
    enum class State { Idle, T1, T2, T3 };

    BusCycle decode_status() const;
    void release_command();
    void on_clk_rising();
    void on_clk_falling();

    // Output pins (we drive these)
    Pin pin_ale_;    // Pin  5: ALE
    Pin pin_den_;    // Pin  4: ~DEN (active low)
    Pin pin_dtr_;    // Pin 16: DT/~R
    Pin pin_memr_;   // Pin  7: ~MEMR (active low)
    Pin pin_memw_;   // Pin  8: ~MEMW (active low)
    Pin pin_ior_;    // Pin 13: ~IOR (active low)
    Pin pin_iow_;    // Pin 12: ~IOW (active low)
    Pin pin_inta_;   // Pin 14: ~INTA (active low)

    // Input pins (we read / subscribe to these)
    Pin pin_clk_;    // Pin  2: CLK
    Pin pin_s0_;     // Pin 19: ~S0
    Pin pin_s1_;     // Pin  3: ~S1
    Pin pin_s2_;     // Pin 18: ~S2
    Pin pin_cen_;    // Pin  6: CEN (command enable)
    Pin pin_aen_;    // Pin 15: ~AEN

    // Internal state
    State state_ = State::Idle;
    BusCycle cycle_ = BusCycle::Passive;
};

} // namespace bench
