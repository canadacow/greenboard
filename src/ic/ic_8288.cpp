#include "ic/ic_8288.h"
#include "ic/ic_74s245.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8288::IC_8288() : CallbackComponent("8288") { set_description("Bus Controller"); }

void IC_8288::on_power_on() {
    state_ = State::Idle;
    cycle_ = BusCycle::Passive;
    release_command();
    // U14 always copies CPU -> X-bus in CPU mode (A->B).
    // Prime it at power-on so command signals propagate from the first cycle.
    if (xcvr_c_) xcvr_c_->set_driving(IC_74S245::Driving::B);
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

    // Pin directions for wiring visualization.
    declare_input(pin_clk_);
    declare_input(pin_s0_); declare_input(pin_s1_);
    declare_input(pin_s2_);
    declare_input(pin_cen_); declare_input(pin_aen_);
    declare_output(pin_ale_); declare_output(pin_den_); declare_output(pin_dtr_);
    declare_output(pin_memr_); declare_output(pin_memw_);
    declare_output(pin_ior_); declare_output(pin_iow_); declare_output(pin_inta_);
}

void IC_8288::on_signal_change(Fiber /*caller*/) {
    on_clk_rising();
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
void IC_8288::set_xcvr(IC_74S245* u8, IC_74S245* u13, IC_74S245* u12, IC_74S245* u14) {
    xcvr_ = u8;
    xcvr_x_ = u13;
    xcvr_m_ = u12;
    xcvr_c_ = u14;
}

void IC_8288::disable_xcvr() {
    if (xcvr_)   xcvr_->set_driving(IC_74S245::Driving::None);
    if (xcvr_x_) xcvr_x_->set_driving(IC_74S245::Driving::None);
    if (xcvr_m_) xcvr_m_->set_driving(IC_74S245::Driving::None);
    // U14 (cmd xcvr) stays enabled -- CPU always drives command bus.
}

void IC_8288::nudge_xcvr() {
    // Pre-set transceiver directions so they copy on the correct eval,
    // before the bidir lambda catches up.
    bool is_write = (pin_dtr_.level() == Level::High);
    auto dir = is_write ? IC_74S245::Driving::B : IC_74S245::Driving::A;
    if (xcvr_)   xcvr_->set_driving(dir);
    if (xcvr_x_) xcvr_x_->set_driving(dir);
    // U12: DIR=~XMEMR.  Memory read -> ~XMEMR=Low -> DIR=Low -> B->A (MD->D).
    // Memory write -> ~XMEMR=High -> DIR=High -> A->B (D->MD).
    // Only nudge for memory cycles AND only when ~RAM_ADDR_SEL is Low (RAM address).
    // For ROM addresses, ~RAM_ADDR_SEL is High and U12 must stay off.
    if (xcvr_m_) {
        bool ram_selected = pin_ram_addr_sel_.level() == Level::Low;
        if (ram_selected && (cycle_ == BusCycle::Fetch || cycle_ == BusCycle::MemR))
            xcvr_m_->set_driving(IC_74S245::Driving::A);  // B->A (MD->D)
        else if (ram_selected && cycle_ == BusCycle::MemW)
            xcvr_m_->set_driving(IC_74S245::Driving::B);  // A->B (D->MD)
        else
            xcvr_m_->set_driving(IC_74S245::Driving::None);  // I/O or ROM: disable
    }
    // U14: CPU mode -> A->B (8288 commands to X-side)
    if (xcvr_c_)
        xcvr_c_->set_driving(IC_74S245::Driving::B);
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
            disable_xcvr();
            state_ = State::Idle;
            cycle_ = BusCycle::Passive;
        }
        return;
    }

    auto cyc_name = [](BusCycle c) -> const char* {
        if (c == BusCycle::Passive) return "Passive";
        if (c == BusCycle::Halt)    return "Halt";
        if (c == BusCycle::Fetch)   return "Fetch";
        if (c == BusCycle::MemR)    return "MemR";
        if (c == BusCycle::MemW)    return "MemW";
        if (c == BusCycle::IOR)     return "IOR";
        if (c == BusCycle::IOW)     return "IOW";
        if (c == BusCycle::INTA)    return "INTA";
        return "???";
    };

    switch (state_) {
        case State::Idle:
        idle_recheck:
        {
            BusCycle bus = decode_status();
            if (bus != BusCycle::Passive && bus != BusCycle::Halt) {
                cycle_ = bus;
                state_ = State::T1;

                bool is_write = (bus == BusCycle::IOW || bus == BusCycle::MemW);
                pin_ale_.drive_immediate(Level::High);
                pin_dtr_.drive_immediate(is_write ? Level::High : Level::Low);
                spdlog::trace("[{}] Idle->T1 cycle={} DT/~R={}", name(), cyc_name(bus), is_write ? "H(wr)" : "L(rd)");
            }
            break;
        }

        case State::T1: {
            state_ = State::T2;
            pin_ale_.drive_immediate(Level::Low);
            // Assert command strobe and ~DEN (was deferred to falling edge).
            {
                bool cen = pin_cen_.level() == Level::High;
                if (cen) {
                    switch (cycle_) {
                        case BusCycle::INTA:  pin_inta_.drive_immediate(Level::Low); break;
                        case BusCycle::IOR:   pin_ior_.drive_immediate(Level::Low);  break;
                        case BusCycle::IOW:   pin_iow_.drive_immediate(Level::Low);  break;
                        case BusCycle::Fetch:
                        case BusCycle::MemR:  pin_memr_.drive_immediate(Level::Low); break;
                        case BusCycle::MemW:  pin_memw_.drive_immediate(Level::Low); break;
                        default: break;
                    }
                }
                spdlog::trace("[{}] T1->T2 cycle={} cmd asserted CEN={}", name(), cyc_name(cycle_), cen);
                pin_den_.drive_immediate(Level::Low);
            }
            nudge_xcvr();
            break;
        }

        case State::T2:
            // T3: Commands stay active. Re-nudge transceivers so reads
            // pick up data that peripherals drove onto XD in the previous eval.
            state_ = State::T3;
            nudge_xcvr();
            spdlog::trace("[{}] T2->T3 cycle={}", name(), cyc_name(cycle_));
            break;

        case State::T3: {
            // T4: Deassert commands, deassert ~DEN, back to idle.
            release_command();
            pin_den_.drive_immediate(Level::High);  // ~DEN deasserted
            disable_xcvr();
            spdlog::trace("[{}] T3->T4(Idle) cycle={} cmds released", name(), cyc_name(cycle_));
            state_ = State::Idle;
            cycle_ = BusCycle::Passive;
            // Back-to-back bus cycles: T4 of one overlaps T1 of the next.
            // Re-check status immediately for a new bus cycle.
            goto idle_recheck;
        }
    }
}

} // namespace bench
