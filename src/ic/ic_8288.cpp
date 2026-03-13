#include "ic/ic_8288.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8288::IC_8288() : BusControllerComponent("8288") {}

void IC_8288::on_power_on() {
    state_ = State::Idle;
    cycle_ = BusCycle::Passive;
}

void IC_8288::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    // Output pins (we drive these).
    pin_ale_  = pin(5);    // ALE
    pin_den_  = pin(4);    // ~DEN (active low)
    pin_dtr_  = pin(16);   // DT/~R
    pin_memr_ = pin(7);    // ~MEMR (active low)
    pin_memw_ = pin(8);    // ~MEMW (active low)
    pin_ior_  = pin(13);   // ~IOR (active low)
    pin_iow_  = pin(12);   // ~IOW (active low)
    pin_inta_ = pin(14);   // ~INTA (active low)

    // Input pins (we subscribe to these).
    pin_clk_  = connect_pin(2);    // CLK
    pin_s0_   = connect_pin(19);   // ~S0
    pin_s1_   = connect_pin(3);    // ~S1
    pin_s2_   = connect_pin(18);   // ~S2
    pin_cen_  = connect_pin(6);    // CEN (command enable)
    pin_aen_  = connect_pin(15);   // ~AEN (active low)

    Signal* vcc = socket.pin_signal(20);
    if (vcc) vcc->connect(this);
}

void IC_8288::on_signal_change(bool rising, bool falling) {
    if (rising) on_clk_rising();
    if (falling) on_clk_falling();
}

IC_8288::BusCycle IC_8288::decode_status() const {
    // Status lines are active low from the 8088.
    bool s0 = pin_s0_.level() == Level::Low;   // active
    bool s1 = pin_s1_.level() == Level::Low;   // active
    bool s2 = pin_s2_.level() == Level::Low;   // active

    //  ~S2 ~S1 ~S0 | Cycle
    //   0   0   0  | INTA     (s2=1, s1=1, s0=1 active)
    //   0   0   1  | IOR      (s2=1, s1=1, s0=0)
    //   0   1   0  | IOW      (s2=1, s1=0, s0=1)
    //   0   1   1  | Halt     (s2=1, s1=0, s0=0)
    //   1   0   0  | Fetch    (s2=0, s1=1, s0=1)
    //   1   0   1  | MemR     (s2=0, s1=1, s0=0)
    //   1   1   0  | MemW     (s2=0, s1=0, s0=1)
    //   1   1   1  | Passive  (s2=0, s1=0, s0=0)
    //
    // The table uses pin-level values (0=Low on pin = active).
    // Our booleans s0/s1/s2 are true when the pin is active (Low).
    int code = (s2 ? 4 : 0) | (s1 ? 2 : 0) | (s0 ? 1 : 0);
    switch (code) {
        case 7: return BusCycle::INTA;    // all active
        case 6: return BusCycle::IOR;     // s2+s1 active, s0 inactive
        case 5: return BusCycle::IOW;     // s2+s0 active, s1 inactive
        case 4: return BusCycle::Halt;    // s2 active only
        case 3: return BusCycle::Fetch;   // s1+s0 active, s2 inactive
        case 2: return BusCycle::MemR;    // s1 active only
        case 1: return BusCycle::MemW;    // s0 active only
        default: return BusCycle::Passive; // none active
    }
}
void IC_8288::release_command() {
    // Deassert all command strobes (active low -> High).
    pin_memr_.drive_immediate(Level::High);
    pin_memw_.drive_immediate(Level::High);
    pin_ior_.drive_immediate(Level::High);
    pin_iow_.drive_immediate(Level::High);
    pin_inta_.drive_immediate(Level::High);
}

void IC_8288::on_clk_rising() {
    // If ~AEN is active (Low), DMA owns the bus -- 8288 is inhibited.
    if (pin_aen_.level() == Level::Low) {
        if (state_ != State::Idle) {
            release_command();
            pin_ale_.drive_immediate(Level::Low);
            pin_den_.drive_immediate(Level::High);  // ~DEN deasserted
            state_ = State::Idle;
            cycle_ = BusCycle::Passive;
        }
        return;
    }

    switch (state_) {
        case State::Idle: {
            BusCycle bus = decode_status();
            if (bus != BusCycle::Passive && bus != BusCycle::Halt) {
                cycle_ = bus;
                state_ = State::T1;

                // T1: Assert ALE, set DT/~R direction.
                bool is_write = (bus == BusCycle::IOW || bus == BusCycle::MemW);
                pin_ale_.drive_immediate(Level::High);
                pin_dtr_.drive_immediate(is_write ? Level::High : Level::Low);
            }
            break;
        }

        case State::T1: {
            state_ = State::T2;

            // T2 CLK rise: Deassert ALE (latches capture on falling edge).
            // ~DEN and command strobe are deferred to T2 CLK fall to avoid
            // bus contention: the 74S245 must not drive AD while the 74S373
            // is capturing the address from AD.
            pin_ale_.drive_immediate(Level::Low);
            break;
        }

        case State::T2:
            // T3: Commands stay active. Nothing changes.
            state_ = State::T3;
            break;

        case State::T3: {
            // T4: Deassert commands, deassert ~DEN, back to idle.
            release_command();
            pin_den_.drive_immediate(Level::High);  // ~DEN deasserted
            state_ = State::Idle;
            cycle_ = BusCycle::Passive;
            break;
        }
    }
}

void IC_8288::on_clk_falling() {
    // Deferred from T2 CLK rise: assert ~DEN and command strobe.
    // This gives the 74S373 a full half-cycle to capture the address
    // before the 74S245 transceiver enables and drives the AD bus.
    if (state_ == State::T2 && pin_den_.level() != Level::Low) {
        bool cen = pin_cen_.level() == Level::High;
        if (cen) {
            // drive_command
            // Assert the appropriate command strobe (active low).
            switch (cycle_) {
                case BusCycle::INTA:
                    pin_inta_.drive_immediate(Level::Low);
                    break;
                case BusCycle::IOR:
                    pin_ior_.drive_immediate(Level::Low);
                    break;
                case BusCycle::IOW:
                    pin_iow_.drive_immediate(Level::Low);
                    break;
                case BusCycle::Fetch:
                case BusCycle::MemR:
                    pin_memr_.drive_immediate(Level::Low);
                    break;
                case BusCycle::MemW:
                    pin_memw_.drive_immediate(Level::Low);
                    break;
                default:
                    break;
            }
        }
        pin_den_.drive_immediate(Level::Low);
    }
}

} // namespace bench
