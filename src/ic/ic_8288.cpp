#include "ic/ic_8288.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8288::IC_8288() : Component("8288") {}

void IC_8288::on_power_on() {
    state_ = State::Idle;
    cycle_ = BusCycle::Passive;
    clk_prev_ = Level::HiZ;
}

void IC_8288::install(Socket& socket) {
    // Output pins (we drive these).
    pin_ale_  = socket.pin_signal(5);   // ALE
    pin_den_  = socket.pin_signal(4);   // ~DEN (active low)
    pin_dtr_  = socket.pin_signal(16);  // DT/~R
    pin_memr_ = socket.pin_signal(7);   // ~MEMR (active low)
    pin_memw_ = socket.pin_signal(8);   // ~MEMW (active low)
    pin_ior_  = socket.pin_signal(13);  // ~IOR (active low)
    pin_iow_  = socket.pin_signal(12);  // ~IOW (active low)
    pin_inta_ = socket.pin_signal(14);  // ~INTA (active low)

    // Input pins (we subscribe to these).
    pin_clk_  = socket.pin_signal(2);   // CLK
    pin_s0_   = socket.pin_signal(19);  // ~S0
    pin_s1_   = socket.pin_signal(3);   // ~S1
    pin_s2_   = socket.pin_signal(18);  // ~S2
    pin_cen_  = socket.pin_signal(6);   // CEN (command enable)
    pin_aen_  = socket.pin_signal(15);  // ~AEN (active low)
    pin_vcc_  = socket.pin_signal(20);  // VCC

    // Subscribe to input signals for mailbox events.
    if (pin_clk_) pin_clk_->connect(this);
    if (pin_s0_)  pin_s0_->connect(this);
    if (pin_s1_)  pin_s1_->connect(this);
    if (pin_s2_)  pin_s2_->connect(this);
    if (pin_cen_) pin_cen_->connect(this);
    if (pin_aen_) pin_aen_->connect(this);
    if (pin_vcc_) pin_vcc_->connect(this);

    spdlog::debug("[8288] installed into socket {}", socket.ref());
}

void IC_8288::on_signal_change() {
    if (pin_clk_) {
        Level cur = pin_clk_->level();
        if (cur == Level::High && clk_prev_ != Level::High)
            on_clk_rising();
        clk_prev_ = cur;
    }
}

IC_8288::BusCycle IC_8288::decode_status() const {
    // Status lines are active low from the 8088.
    bool s0 = pin_s0_ && pin_s0_->level() == Level::Low;   // active
    bool s1 = pin_s1_ && pin_s1_->level() == Level::Low;   // active
    bool s2 = pin_s2_ && pin_s2_->level() == Level::Low;   // active

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

void IC_8288::drive_command(BusCycle cycle) {
    // Assert the appropriate command strobe (active low).
    switch (cycle) {
        case BusCycle::INTA:
            if (pin_inta_) pin_inta_->drive(Level::Low);
            break;
        case BusCycle::IOR:
            if (pin_ior_) pin_ior_->drive(Level::Low);
            break;
        case BusCycle::IOW:
            if (pin_iow_) pin_iow_->drive(Level::Low);
            break;
        case BusCycle::Fetch:
        case BusCycle::MemR:
            if (pin_memr_) pin_memr_->drive(Level::Low);
            break;
        case BusCycle::MemW:
            if (pin_memw_) pin_memw_->drive(Level::Low);
            break;
        default:
            break;
    }
}

void IC_8288::release_command() {
    // Deassert all command strobes (active low -> High).
    if (pin_memr_) pin_memr_->drive(Level::High);
    if (pin_memw_) pin_memw_->drive(Level::High);
    if (pin_ior_)  pin_ior_->drive(Level::High);
    if (pin_iow_)  pin_iow_->drive(Level::High);
    if (pin_inta_) pin_inta_->drive(Level::High);
}

void IC_8288::on_clk_rising() {
    // If ~AEN is active (Low), DMA owns the bus -- 8288 is inhibited.
    if (pin_aen_ && pin_aen_->level() == Level::Low) {
        if (state_ != State::Idle) {
            release_command();
            if (pin_ale_) pin_ale_->drive(Level::Low);
            if (pin_den_) pin_den_->drive(Level::High);  // ~DEN deasserted
            state_ = State::Idle;
            cycle_ = BusCycle::Passive;
        }
        return;
    }

    auto cycle_name = [](BusCycle c) -> const char* {
        switch (c) {
            case BusCycle::Passive: return "Passive";
            case BusCycle::INTA: return "INTA";
            case BusCycle::IOR: return "IOR";
            case BusCycle::IOW: return "IOW";
            case BusCycle::Halt: return "Halt";
            case BusCycle::Fetch: return "Fetch";
            case BusCycle::MemR: return "MemR";
            case BusCycle::MemW: return "MemW";
        }
        return "?";
    };

    auto state_name = [](State s) -> const char* {
        switch (s) {
            case State::Idle: return "Idle";
            case State::T1: return "T1";
            case State::T2: return "T2";
            case State::T3: return "T3";
        }
        return "?";
    };

    spdlog::trace("[8288] CLK_RISE state={} cycle={}", state_name(state_), cycle_name(cycle_));

    switch (state_) {
        case State::Idle: {
            BusCycle bus = decode_status();
            if (bus != BusCycle::Passive && bus != BusCycle::Halt) {
                cycle_ = bus;
                state_ = State::T1;

                // T1: Assert ALE, set DT/~R direction.
                if (pin_ale_) pin_ale_->drive(Level::High);

                // DT/~R: High = transmit (write), Low = receive (read).
                bool is_write = (bus == BusCycle::IOW || bus == BusCycle::MemW);
                if (pin_dtr_) pin_dtr_->drive(is_write ? Level::High : Level::Low);
                spdlog::trace("[8288] Idle->T1 cycle={} DT/~R={}", cycle_name(bus), is_write ? "TX" : "RX");
            }
            break;
        }

        case State::T1: {
            state_ = State::T2;

            // T2: Deassert ALE, drive command strobe, assert ~DEN.
            if (pin_ale_) pin_ale_->drive(Level::Low);

            // Only drive commands if CEN is High (command enable).
            bool cen = !pin_cen_ || pin_cen_->level() == Level::High;
            if (cen) {
                drive_command(cycle_);
            }

            // ~DEN active (Low) = data bus transceivers enabled.
            if (pin_den_) pin_den_->drive(Level::Low);
            spdlog::trace("[8288] T1->T2 ~DEN=Low (enabled), cmd={}", cycle_name(cycle_));
            break;
        }

        case State::T2:
            // T3: Commands stay active. Nothing changes.
            state_ = State::T3;
            spdlog::trace("[8288] T2->T3");
            break;

        case State::T3:
            // T4: Deassert commands, deassert ~DEN, back to idle.
            release_command();
            if (pin_den_) pin_den_->drive(Level::High);  // ~DEN deasserted
            spdlog::trace("[8288] T3->Idle ~DEN=High (disabled)");
            state_ = State::Idle;
            cycle_ = BusCycle::Passive;
            break;
    }
}

} // namespace bench
