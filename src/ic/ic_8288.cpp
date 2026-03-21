#include "ic/ic_8288.h"
#include "ic/ic_74s245.h"
#include "ic/ic_74s373.h"
#include "ic/ic_74ls670.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8288::IC_8288() : CallbackComponent("8288") { set_description("Bus Controller"); }

void IC_8288::on_power_on() {
    cycle_ = BusCycle::Passive;
    prev_active_ = false;
    commanding_ = false;
    inhibited_ = false;
    release_command();
    // U14 always copies CPU -> X-bus in CPU mode (A->B).
    // Prime it at power-on so command signals propagate from the first cycle.
    xcvr_c_->set_driving(IC_74S245::Driving::B);
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
    // CEN/AEN come from U98 (clocked FF). Sync input so 8288 evaluates
    // after U98 and sees the current cycle's AEN_BRD/~AEN state.
    // No DAG cycle: U98's inputs (HOLDA, AEN_BRD, etc.) don't come from 8288.
    declare_async_input(pin_cen_); declare_input(pin_aen_);
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

void IC_8288::set_addr_latches(IC_74S373* u18, IC_74LS670* u19) {
    u18_ = u18;
    u19_ = u19;
}

void IC_8288::disable_xcvr() {
    xcvr_->set_driving(IC_74S245::Driving::None);
    xcvr_x_->set_driving(IC_74S245::Driving::None);
    xcvr_m_->set_driving(IC_74S245::Driving::None);
    // U14 (cmd xcvr) stays enabled -- CPU always drives command bus.
}

void IC_8288::nudge_xcvr() {
    // Pre-set transceiver directions so they copy on the correct eval,
    // before the bidir lambda catches up.
    bool is_write = (cycle_ == BusCycle::IOW || cycle_ == BusCycle::MemW);
    auto dir = is_write ? IC_74S245::Driving::B : IC_74S245::Driving::A;
    xcvr_->set_driving(dir);
    xcvr_x_->set_driving(dir);
    // U12: DIR=~XMEMR.  Memory read -> ~XMEMR=Low -> DIR=Low -> B->A (MD->D).
    // Memory write -> ~XMEMR=High -> DIR=High -> A->B (D->MD).
    // Only nudge for memory cycles AND only when ~RAM_ADDR_SEL is Low (RAM address).
    // For ROM addresses, ~RAM_ADDR_SEL is High and U12 must stay off.

    bool ram_selected = (pin_a18_.level() != Level::High && pin_a19_.level() != Level::High);
    if (ram_selected && (cycle_ == BusCycle::Fetch || cycle_ == BusCycle::MemR))
        xcvr_m_->set_driving(IC_74S245::Driving::A);  // B->A (MD->D)
    else if (ram_selected && cycle_ == BusCycle::MemW)
        xcvr_m_->set_driving(IC_74S245::Driving::B);  // A->B (D->MD)
    else
        xcvr_m_->set_driving(IC_74S245::Driving::None);  // I/O or ROM: disable
    // U14: CPU mode -> A->B (8288 commands to X-side)
    xcvr_c_->set_driving(IC_74S245::Driving::B);
}

void IC_8288::release_command() {
    // Deassert all command strobes (active low -> High).
    pin_memr_.drive(Level::High);
    pin_memw_.drive(Level::High);
    pin_ior_.drive(Level::High);
    pin_iow_.drive(Level::High);
    pin_inta_.drive(Level::High);
}

void IC_8288::on_clk_rising() {
    // If ~AEN is active (Low), DMA owns the bus -- 8288 is inhibited.
    // Per 82C88 datasheet: CEN LOW forces command outputs inactive but
    // does NOT reset the internal state machine.  Preserve state_/cycle_
    // so the command re-asserts when DMA releases the bus.
    // pin_aen_ (pin 15) = ~RDY/WAIT from U82 FF2 Q (synced with 8284A ~AEN1).
    // pin_cen_ (pin 6)  = AEN_BRD from U98 1Q (DMA bus ownership).
    // Inhibit when DMA owns bus (AEN_BRD High) AND wait state active (~RDY/WAIT Low).
    // When U82 Q goes High, ~RDY/WAIT=High, un-inhibit so 8288 can re-assert
    // commands in the same cycle READY goes High.
    auto cyc_str = [this]() -> const char* {
        switch (cycle_) {
            case BusCycle::Passive: return "Passive"; case BusCycle::INTA: return "INTA";
            case BusCycle::IOR: return "IOR"; case BusCycle::IOW: return "IOW";
            case BusCycle::Halt: return "Halt"; case BusCycle::Fetch: return "Fetch";
            case BusCycle::MemR: return "MemR"; case BusCycle::MemW: return "MemW";
            default: return "???";
        }
    };

    spdlog::trace("[{}] entry: ~AEN={} CEN={} inhibited={} commanding={} bus_hold={} cycle={}",
                  name(), int(pin_aen_.level()), int(pin_cen_.level()),
                  inhibited_, commanding_, bus_hold_, cyc_str());

    // --- Bus recovery state machine (B0-B4) ---
    // While bus_hold_ > 0, DMA re-inhibit is blocked.
    // bus_hold() (checked by 8284A) returns true when bus_hold_ > 0.
    if (bus_hold_ > 0) {
        // Don't advance bus recovery until CEN goes High (AEN_BRD dropped,
        // CPU address latches enabled). Stall at current count.
        if (pin_cen_.level() != Level::High) {
            spdlog::trace("[{}] bus recovery stalled: CEN={} bus_hold={}", name(), int(pin_cen_.level()), bus_hold_);
            return;
        }
        bus_hold_--;
        if (bus_hold_ == 2) {
            // B1: un-inhibit, re-assert commands, nudge transceivers.
            inhibited_ = false;
            spdlog::trace("[{}] *** B1: un-inhibit, re-assert {} -- READY held Low ***", name(), cyc_str());
            if (cycle_ != BusCycle::Passive && cycle_ != BusCycle::Halt) {
                commanding_ = true;
                switch (cycle_) {
                    case BusCycle::INTA:  pin_inta_.drive(Level::Low); break;
                    case BusCycle::IOR:   pin_ior_.drive(Level::Low);  break;
                    case BusCycle::IOW:   pin_iow_.drive(Level::Low);  break;
                    case BusCycle::Fetch:
                    case BusCycle::MemR:  pin_memr_.drive(Level::Low); break;
                    case BusCycle::MemW:  pin_memw_.drive(Level::Low); break;
                    default: break;
                }
                pin_den_.drive(Level::Low);
                nudge_xcvr();
            }
            return;
        } else if (bus_hold_ == 1) {
            // B2: CAS falls. Nudge transceivers again.
            spdlog::trace("[{}] *** B3: CAS settling, nudge again -- READY held Low ***", name());
            nudge_xcvr();
            return;
        } else {
            // B3: bus_hold_==0. DRAM reads. CPU reads. DMA unblocked.
            spdlog::trace("[{}] *** B4: bus settled, READY+DMA released ***", name());
            // Fall through to normal processing.
        }
    }

    // Normal inhibit check (skipped while bus_hold_ > 0 = bus recovery active).
    if (bus_hold_ == 0) {
        bool dma_owns_bus = pin_cen_.level() == Level::Low;
        bool wait_active  = pin_aen_.level() == Level::Low;
        bool should_inhibit = dma_owns_bus && wait_active;
        if (should_inhibit) {
            if (!inhibited_) {
                spdlog::trace("[{}] *** INHIBIT: DMA taking bus (commanding={} cycle={}) ***", name(), commanding_, cyc_str());
                release_command();
                pin_ale_.drive(Level::Low);
                pin_den_.drive(Level::High);
                disable_xcvr();
                inhibited_ = true;
            }
            return;
        }
    }

    // Detect un-inhibit trigger: inhibited but should_inhibit is false.
    // Start B0 of bus recovery.
    if (inhibited_ && bus_hold_ == 0) {
        bus_hold_ = 3;  // B0: do nothing this cycle, 3 cycles to full recovery
        // Release DMA address latches so their bidirs revert to pin-driven
        u18_->set_dma_output(false);
        u19_->set_dma_output(false);
        spdlog::trace("[{}] *** B0: bus recovery STARTED (cycle={}) -- DMA write finishing ***", name(), cyc_str());
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

    BusCycle bus = decode_status();
    bool active = (bus != BusCycle::Passive && bus != BusCycle::Halt);

    if (!prev_active_ && active) {
        // passive->active: new bus cycle (T1).
        // If commands are held from previous cycle, release them first.
        if (commanding_) {
            release_command();
            pin_den_.drive(Level::High);
            disable_xcvr();
            commanding_ = false;
            spdlog::trace("[{}] cmds released (prev cycle={})", name(), cyc_name(cycle_));
        }
        cycle_ = bus;
        bool is_write = (bus == BusCycle::IOW || bus == BusCycle::MemW);
        pin_ale_.drive(Level::High);
        pin_dtr_.drive(is_write ? Level::High : Level::Low);
        spdlog::trace("[{}] T1 cycle={} DT/~R={}", name(), cyc_name(bus), is_write ? "H(wr)" : "L(rd)");

    } else if (prev_active_ && active) {
        // active->active: T2. ALE falls, assert command, enable data.
        pin_ale_.drive(Level::Low);
        bool cen = pin_cen_.level() == Level::High;
        if (cen && !commanding_) {
            switch (cycle_) {
                case BusCycle::INTA:  pin_inta_.drive(Level::Low); break;
                case BusCycle::IOR:   pin_ior_.drive(Level::Low);  break;
                case BusCycle::IOW:   pin_iow_.drive(Level::Low);  break;
                case BusCycle::Fetch:
                case BusCycle::MemR:  pin_memr_.drive(Level::Low); break;
                case BusCycle::MemW:  pin_memw_.drive(Level::Low); break;
                default: break;
            }
            commanding_ = true;
        }
        pin_den_.drive(Level::Low);
        nudge_xcvr();
        spdlog::trace("[{}] T2 cycle={} cmd asserted CEN={}", name(), cyc_name(cycle_), cen);

    } else if (prev_active_ && !active) {
        // active->passive: entering T3. Commands stay active, nudge xcvrs.
        nudge_xcvr();
        spdlog::trace("[{}] T3 cycle={}", name(), cyc_name(cycle_));

    } else {
        // passive->passive: Tw. Hold commands.
    }

    prev_active_ = active;
}

} // namespace bench
