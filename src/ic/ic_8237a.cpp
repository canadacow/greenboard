#include "ic/ic_8237a.h"
#include "ic/ic_74s245.h"
#include "ic/ic_74s373.h"
#include "ic/ic_74ls670.h"
#include "ic/ic_8288.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8237A::IC_8237A() : CallbackComponent("8237A") { set_description("DMA"); }

void IC_8237A::set_xcvr(IC_74S245* u8, IC_74S245* u12, IC_74S245* u13, IC_74S245* u14) {
    xcvr_ = u8;
    xcvr_m_ = u12;
    xcvr_x_ = u13;
    xcvr_c_ = u14;
}

void IC_8237A::set_addr_latches(IC_74S373* u18, IC_74LS670* u19) {
    u18_ = u18;
    u19_ = u19;
}

void IC_8237A::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    // Data bus
    pin_db_[0] = pin(30); pin_db_[1] = pin(29); pin_db_[2] = pin(28); pin_db_[3] = pin(27);
    pin_db_[4] = pin(26); pin_db_[5] = pin(23); pin_db_[6] = pin(22); pin_db_[7] = pin(21);

    // Address
    pin_a_[0] = pin(32); pin_a_[1] = pin(33); pin_a_[2] = pin(34); pin_a_[3] = pin(35);
    pin_a_[4] = pin(37); pin_a_[5] = pin(38); pin_a_[6] = pin(39); pin_a_[7] = pin(40);

    // Control
    pin_ior_   = connect_pin(1);
    pin_iow_   = connect_pin(2);
    pin_cs_    = connect_pin(11);
    pin_clk_   = connect_pin(12);
    pin_reset_ = connect_pin(13);
    pin_ready_ = pin(6);
    pin_hlda_  = connect_pin(7);
    pin_eop_   = pin(36);
    pin_vcc_   = connect_pin(5);

    // DREQ inputs
    for (int i = 0; i < 4; ++i)
        pin_dreq_[i] = connect_pin(19 - i);

    // Outputs
    pin_hrq_   = pin(10);
    pin_dack_[0] = pin(25); pin_dack_[1] = pin(24); pin_dack_[2] = pin(14); pin_dack_[3] = pin(15);
    pin_memr_  = pin(3);
    pin_memw_  = pin(4);
    pin_adstb_ = pin(8);
    pin_aen_   = pin(9);

    // Pin directions for wiring visualization / DAG construction.
    declare_input(pin_ior_); declare_input(pin_iow_);
    // ~CS: Input during CPU mode (for register programming), HiZ during DMA
    // to break DAG cycle (U35 -> DACK/addr -> decode chain -> U66 -> ~DMA_CS -> U35).
    declare_bidir_block({pin_cs_},
        BidirDir::Input | BidirDir::HiZ,
        [this]() { return (state_ != State::SI) ? BidirDir::HiZ : BidirDir::Input; });
    declare_async_input(pin_clk_);   // DCLK: clock input, no combinational dependency
    declare_input(pin_reset_);
    declare_async_input(pin_hlda_);  // HLDA: clocked by U67, no same-cycle feedback from HRQ
    for (int i = 0; i < 4; ++i) declare_async_input(pin_dreq_[i]);  // async DMA requests
    declare_output(pin_hrq_); declare_output(pin_eop_);
    for (int i = 0; i < 4; ++i) declare_output(pin_dack_[i]);
    declare_output(pin_adstb_); declare_output(pin_aen_);

    // DMA-only outputs: HiZ in CPU mode, Output during DMA.
    // Removes false DAG edges in CPU-mode perms (HRQ->...->HOLDA->8237A cycle).
    // ~EOP excluded: its pulse must persist across the SI transition
    // (eop_pending_ deasserts it next cycle).
    // DACKs included: bidir HiZ removes DAG edges in CPU mode, but
    // on_signal_change re-drives them High so downstream enables (U48 G1)
    // see a stable High rather than floating HiZ.
    // HRQ: Output only during BusRequested (asserting HRQ), HiZ during
    // active DMA (S1-S4) since HRQ stays stable -- avoids DAG cycle
    // (U35 HRQ -> U99 -> U52 -> CEN -> U5 -> ... -> U6-8288 -> U35).
    declare_bidir_block({pin_hrq_, pin_eop_},
        BidirDir::HiZ | BidirDir::Output,
        [this]() { return state_ == State::BusRequested ? BidirDir::Output : BidirDir::HiZ; });
    // DACKs, ADSTB, AEN: Output during active DMA transfers.
    declare_bidir_block({pin_dack_[0], pin_dack_[1],
                         pin_dack_[2], pin_dack_[3], pin_adstb_, pin_aen_},
        BidirDir::HiZ | BidirDir::Output,
        [this]() { return is_dma_active() ? BidirDir::Output : BidirDir::HiZ; });

    // Address pins: input during CPU I/O (register select), output only during
    // DMA transfer.  No declare_input -- the bidir block handles Input direction.
    // A separate declare_input would create persistent DAG edges that cause
    // cycles when the bidir switches to Output during DMA.
    declare_bidir_block({pin_a_[0], pin_a_[1], pin_a_[2], pin_a_[3],
                         pin_a_[4], pin_a_[5], pin_a_[6], pin_a_[7]},
        BidirDir::Input | BidirDir::Output,
        [this]() { return is_dma_active() ? BidirDir::Output : BidirDir::Input; });

    // Data bus: output during CPU reads of DMA registers (~CS+~IOR active),
    // or S1 (upper addr on DB).  During DMA S2-S4 the 8237A reads device data
    // from DB but this is not combinational (data set up in prior phase), so
    // use HiZ to avoid DAG cycle (U35 -> ~DACK -> device -> XD -> U35).
    declare_bidir_block({pin_db_[0], pin_db_[1], pin_db_[2], pin_db_[3],
                         pin_db_[4], pin_db_[5], pin_db_[6], pin_db_[7]},
        BidirDir::Input | BidirDir::Output | BidirDir::HiZ,
        [this]() {
            if (state_ == State::S1) return BidirDir::Output;
            if (is_dma_active()) return BidirDir::HiZ;  // async read during DMA
            return (pin_cs_.level() == Level::Low && pin_ior_.level() == Level::Low)
                ? BidirDir::Output : BidirDir::Input;
        });

    // ~MEMR/~MEMW: output during DMA transfers, HiZ otherwise.
    // No declare_input -- 8237A never reads these, only drives during DMA.
    declare_bidir_block({pin_memr_, pin_memw_},
        BidirDir::HiZ | BidirDir::Output,
        [this]() { return is_dma_active() ? BidirDir::Output : BidirDir::HiZ; });
}

void IC_8237A::on_signal_change(Fiber /*caller*/) {
    Level reset_cur = pin_reset_.level();
    Level iow_cur = pin_iow_.level();
    Level cs_cur = pin_cs_.level();
    Level ior_cur = pin_ior_.level();
    Level clk_cur = pin_clk_.level();

    // RESET rising edge
    if (reset_cur == Level::High && reset_prev_ != Level::High)
        on_reset();

    // Deferred bus operations: detect falling edge of ~IOW/~IOR + ~CS,
    // execute on the next cycle when data is stable.
    if (write_pending_) {
        on_bus_write();
        write_pending_ = false;
    } else if (iow_cur == Level::Low && cs_cur == Level::Low &&
               !(iow_prev_ == Level::Low && cs_prev_ == Level::Low)) {
        write_pending_ = true;
    }

    if (read_pending_) {
        on_bus_read();
        read_pending_ = false;
    } else if (ior_cur == Level::Low && cs_cur == Level::Low &&
               !(ior_prev_ == Level::Low && cs_prev_ == Level::Low)) {
        read_pending_ = true;
    }

    // Release data bus when read goes inactive
    if (db_driving_ && !(ior_cur == Level::Low && cs_cur == Level::Low))
        release_data();

    // DREQ changes -- check for new DMA requests.
    // evaluate_dreq() gates on HLDA Low per datasheet p.6.
    static constexpr const char* state_names[] = {
        "SI", "S0", "S1", "S2", "S3", "S4", "M2M_S1", "M2M_S2", "M2M_S3", "M2M_S4"
    };
    spdlog::trace("[8237A] DREQ: ch0={} ch1={} ch2={} ch3={} disabled={} state={}",
                  int(pin_dreq_[0].level()), int(pin_dreq_[1].level()),
                  int(pin_dreq_[2].level()), int(pin_dreq_[3].level()),
                  disabled_, state_names[static_cast<int>(state_)]);
    evaluate_dreq();

    // Advance state machine. Each call to on_signal_change is one full cycle, always
    spdlog::debug("[8237A] CLK tick: state={} clk={}/{}", int(state_), int(clk_cur), int(clk_prev_));
    on_clk_falling();
    spdlog::debug("[8237A] CLK post: state={}", int(state_));

    // Re-drive DACKs after bidir HiZ release so they don't float.
    // The bidir block removes DAG edges in CPU mode (HiZ), but downstream
    // enables (e.g. U48 G1 = ~DACK_0_BRD) need a stable High.
    if (!is_dma_active()) {
        for (int i = 0; i < 4; ++i)
            pin_dack_[i].drive(Level::High);
    }

    reset_prev_ = reset_cur;
    iow_prev_ = iow_cur;
    cs_prev_ = cs_cur;
    ior_prev_ = ior_cur;
    clk_prev_ = clk_cur;
    hlda_prev_ = pin_hlda_.level();
}

void IC_8237A::on_reset() {
    command_ = 0;
    status_ = 0;
    temp_ = 0;
    flip_flop_ = false;
    disabled_ = false;
    state_ = State::SI;
    active_ch_ = -1;
    prev_upper_addr_ = 0;
    eop_pending_ = false;

    for (int i = 0; i < 4; ++i) {
        ch_[i].masked = true;
        ch_[i].request = false;
        ch_[i].tc_reached = false;
    }

    // Deassert outputs
    write_pending_ = false;
    read_pending_ = false;
    pin_hrq_.drive(Level::Low); hrq_driven_ = false;
    pin_eop_.drive(Level::High);          // ~EOP inactive (active low)
    for (int i = 0; i < 4; ++i)
        pin_dack_[i].drive(Level::High);  // active low
    pin_aen_.drive(Level::Low);
    pin_adstb_.drive(Level::Low);

    release_data();
    release_address();

    spdlog::debug("[8237A] reset");
}

void IC_8237A::on_bus_write() {
    uint8_t data = read_data();
    // Read A0-A3 for register select
    uint8_t reg = 0;
    for (int i = 0; i < 4; ++i) {
        if (pin_a_[i].level() == Level::High)
            reg |= (1 << i);
    }

    switch (reg) {
        case 0x00: case 0x02: case 0x04: case 0x06: {
            // Channel base/current address (write sets both)
            int ch = reg >> 1;
            if (!flip_flop_) {
                ch_[ch].base_address = (ch_[ch].base_address & 0xFF00) | data;
                ch_[ch].current_address = (ch_[ch].current_address & 0xFF00) | data;
            } else {
                ch_[ch].base_address = (ch_[ch].base_address & 0x00FF) | (data << 8);
                ch_[ch].current_address = (ch_[ch].current_address & 0x00FF) | (data << 8);
            }
            flip_flop_ = !flip_flop_;
            break;
        }
        case 0x01: case 0x03: case 0x05: case 0x07: {
            // Channel base/current word count
            int ch = (reg - 1) >> 1;
            if (!flip_flop_) {
                ch_[ch].base_count = (ch_[ch].base_count & 0xFF00) | data;
                ch_[ch].current_count = (ch_[ch].current_count & 0xFF00) | data;
            } else {
                ch_[ch].base_count = (ch_[ch].base_count & 0x00FF) | (data << 8);
                ch_[ch].current_count = (ch_[ch].current_count & 0x00FF) | (data << 8);
            }
            flip_flop_ = !flip_flop_;
            break;
        }
        case 0x08:  // Command register
            command_ = data;
            disabled_ = (data & 0x04) != 0;
            spdlog::debug("[8237A] command={:#04x} disabled={}", data, disabled_);
            evaluate_dreq();
            break;

        case 0x09: {  // Request register (software request)
            int ch = data & 0x03;
            ch_[ch].request = (data & 0x04) != 0;
            evaluate_dreq();
            break;
        }

        case 0x0A: {  // Single mask register
            int ch = data & 0x03;
            ch_[ch].masked = (data & 0x04) != 0;
            spdlog::debug("[8237A] single mask: ch{}={}", ch, ch_[ch].masked ? "MASKED" : "unmasked");
            evaluate_dreq();
            break;
        }

        case 0x0B: {  // Mode register
            int ch = data & 0x03;
            ch_[ch].mode = data;
            break;
        }

        case 0x0C:  // Clear byte flip-flop
            flip_flop_ = false;
            break;

        case 0x0D:  // Master clear (same as hardware reset)
            on_reset();
            break;

        case 0x0E:  // Clear mask register (unmask all channels)
            for (int i = 0; i < 4; ++i)
                ch_[i].masked = false;
            evaluate_dreq();
            break;

        case 0x0F:  // Write all mask bits
            for (int i = 0; i < 4; ++i)
                ch_[i].masked = (data & (1 << i)) != 0;
            evaluate_dreq();
            break;
    }
}

void IC_8237A::on_bus_read() {
    uint8_t reg = 0;
    for (int i = 0; i < 4; ++i) {
        if (pin_a_[i].level() == Level::High)
            reg |= (1 << i);
    }

    switch (reg) {
        case 0x00: case 0x02: case 0x04: case 0x06: {
            // Current address
            int ch = reg >> 1;
            uint8_t val = flip_flop_ ?
                (ch_[ch].current_address >> 8) : (ch_[ch].current_address & 0xFF);
            drive_data(val);
            flip_flop_ = !flip_flop_;
            break;
        }
        case 0x01: case 0x03: case 0x05: case 0x07: {
            // Current word count
            int ch = (reg - 1) >> 1;
            uint8_t val = flip_flop_ ?
                (ch_[ch].current_count >> 8) : (ch_[ch].current_count & 0xFF);
            drive_data(val);
            flip_flop_ = !flip_flop_;
            break;
        }
        case 0x08: {
            // Status register: bits 0-3 = TC reached (ch0-3), bits 4-7 = DREQ active
            uint8_t s = 0;
            for (int i = 0; i < 4; ++i) {
                if (ch_[i].tc_reached) s |= (1 << i);
                if (pin_dreq_[i].level() == Level::High)
                    s |= (1 << (i + 4));
            }
            drive_data(s);
            // Reading status clears TC flags
            for (int i = 0; i < 4; ++i)
                ch_[i].tc_reached = false;
            break;
        }
        case 0x0D:
            // Temporary register
            drive_data(temp_);
            break;

        default:
            drive_data(0xFF);
            break;
    }
}

void IC_8237A::evaluate_dreq() {
    if (disabled_ || state_ != State::SI) return;

    // Datasheet p.6: "HRQ will go inactive and the 8237A will wait for
    // HLDA to go low before activating HRQ to service another channel."
    if (pin_hlda_.level() == Level::High) return;

    // Don't assert HRQ during bus recovery -- CPU needs the bus to complete
    // its interrupted read/write before DMA can take it again.
    if (bus_ctrl_->bus_hold()) {
        spdlog::trace("[8237A] DREQ blocked by bus_hold (hold={})", bus_ctrl_->bus_hold());
        return;
    }

    // Fixed priority: CH0 highest
    for (int i = 0; i < 4; ++i) {
        if (ch_[i].masked) continue;
        Level pinLevel = pin_dreq_[i].level();

        bool dreq = (pinLevel == Level::High) || ch_[i].request;
        if (dreq) {
            // Assert HRQ, wait for HLDA
            active_ch_ = i;
            state_ = State::BusRequested;
            pin_hrq_.drive(Level::High); hrq_driven_ = true;
            spdlog::debug("[8237A] HRQ asserted for ch{} (HLDA={} bus_hold={} state={})",
                          i, int(pin_hlda_.level()), bus_ctrl_->bus_hold(), int(state_));
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// DMA transfer state machine (datasheet Figure 11)
//
// Each state is one CLK cycle.  Transitions happen on CLK falling edge.
//
//   SI  -- idle, poll DREQ -> BusRequested (HRQ high)
//   BusRequested -- wait for HLDA -> S1
//   S1  -- AEN high, drive A0-A7 + DB (upper addr), ADSTB high, ~DACK low
//   S2  -- ADSTB falls (latches upper addr), release DB, assert ~MEMR/~MEMW
//   S3  -- wait (extends read; skipped if compressed timing bit set)
//   S4  -- deassert strobes, update addr/count, TC check, release or loop
// ---------------------------------------------------------------------------

void IC_8237A::on_clk_falling() {
    // Deassert EOP from previous cycle
    if (eop_pending_) {
        pin_eop_.drive(Level::High);
        eop_pending_ = false;
    }

    switch (state_) {

    case State::SI:
        // Deferred bus release from end_dma_service.
        if (hrq_driven_ && active_ch_ < 0) {
            pin_hrq_.drive(Level::Low); hrq_driven_ = false;
            pin_aen_.drive(Level::Low);
            release_address();
            release_data();
        }
        break;

    case State::BusRequested:
        if (pin_hlda_.level() == Level::High) {
            spdlog::debug("[8237A] HLDA received, entering S1 for ch{}", active_ch_);
            state_ = State::S1;
        } else {
            spdlog::trace("[8237A] BusRequested: waiting for HLDA (HLDA={})", int(pin_hlda_.level()));
            break;
        }
        // Fall through to execute S1 in this same CLK cycle
        [[fallthrough]];

    case State::S1: {
        auto& ch = ch_[active_ch_];

        // AEN high -- signals external logic that DMA owns the bus
        pin_aen_.drive(Level::High);

        // Assert ~DACK for active channel (not during mem-to-mem per datasheet)
        if (!(command_ & 0x01))
            pin_dack_[active_ch_].drive(Level::Low);

        // Drive A0-A7 with lower 8 bits of current address
        for (int i = 0; i < 8; ++i)
            pin_a_[i].drive((ch.current_address >> i) & 1 ? Level::High : Level::Low);
        a_driving_ = true;

        // Drive DB0-DB7 with upper address byte (A8-A15) for 74S373 latch
        uint8_t upper = static_cast<uint8_t>(ch.current_address >> 8);
        drive_data(upper);
        prev_upper_addr_ = upper;

        // ADSTB high -- will fall at S2 entry, latching upper address
        pin_adstb_.drive(Level::High);

        // Nudge transceivers now (S1) so bidir lambdas pick up the
        // correct direction at the start of next evaluate (S2).
        //   U8  (AD<->D): HiZ -- CPU disconnected during DMA
        //   U14 (cmd):    B->A -- DMA's ~MEMR/~MEMW reach system side
        //   U13 (D<->XD): IO->mem write: A (XD->D), mem->IO read: B (D->XD)
        //   U12 (D<->MD): IO->mem write: B (D->MD), mem->IO read: A (MD->D)
        {
            // U8 (AD<->D): CPU disconnected during DMA
            xcvr_->set_driving(IC_74S245::Driving::None);
            // U14 (cmd): B->A so DMA's ~MEMR/~MEMW reach system side
            xcvr_c_->set_driving(IC_74S245::Driving::A);

            if (command_ & 0x01) {
                // Memory-to-memory: read phase (ch0)
                // DRAM -> MD -> U12(MD->D) -> D -> U13(D->XD) -> XD -> 8237A DB
                xcvr_m_->set_driving(IC_74S245::Driving::A);  // MD->D
                xcvr_x_->set_driving(IC_74S245::Driving::B);  // D->XD
            } else {
                // Normal IO DMA
                // U13 must stay off during DMA or it tramples D with stale XD data.
                xcvr_x_->set_driving(IC_74S245::Driving::None);
                // U12 (D<->MD): data path between D bus and DRAM
                uint8_t transfer_type = (ch.mode >> 2) & 0x03;
                if (transfer_type == 0x01) {
                    xcvr_m_->set_driving(IC_74S245::Driving::B);  // D->MD
                } else if (transfer_type == 0x02) {
                    xcvr_m_->set_driving(IC_74S245::Driving::A);  // MD->D
                }
            }
        }

        // Enable DMA address latches so their bidir lambdas return Output
        // at the start of next cycle (S2), before ~DMA_AEN propagates.
        u18_->set_dma_output(true);
        u19_->set_dma_output(true);

        spdlog::debug("[8237A] S1 ch{}: addr={:#06x} upper={:#04x} A0-7=[{}{}{}{}{}{}{}{}]",
                      active_ch_, ch.current_address, upper,
                      int(pin_a_[7].level()), int(pin_a_[6].level()),
                      int(pin_a_[5].level()), int(pin_a_[4].level()),
                      int(pin_a_[3].level()), int(pin_a_[2].level()),
                      int(pin_a_[1].level()), int(pin_a_[0].level()));

        state_ = State::S2;
        break;
    }

    case State::S2: {
        // ADSTB falling edge latches upper address into external 74S373 (U18)
        pin_adstb_.drive(Level::Low);

        // Keep driving DB with upper address through S2 so U18 can latch
        // on ADSTB falling edge. Release deferred to S3.

        auto& ch = ch_[active_ch_];

        if (command_ & 0x01) {
            // Memory-to-memory: read phase uses ~MEMR only.
            pin_memr_.drive(Level::Low);
            // U12: MD->D (read from DRAM onto D bus)
            xcvr_m_->set_driving(IC_74S245::Driving::A);
        } else {
            // Normal DMA: assert strobes based on transfer type
            uint8_t transfer_type = (ch.mode >> 2) & 0x03;
            if (transfer_type == 0x01) {
                pin_memw_.drive(Level::Low);
                pin_ior_.drive(Level::Low);
            } else if (transfer_type == 0x02) {
                pin_memr_.drive(Level::Low);
                pin_iow_.drive(Level::Low);
            }
        }

        xcvr_c_->set_driving(IC_74S245::Driving::A);

        spdlog::debug("[8237A] S2 ch{}: addr={:#06x}{}",
                      active_ch_,
                      ch.current_address,
                      (command_ & 0x01) ? " (m2m read)" : "");

        bool compressed = (command_ & 0x08) != 0;
        if (compressed) {
            // Compressed timing skips S3; release DB and check READY here.
            release_data();
            state_ = (pin_ready_.level() == Level::High) ? State::S4 : State::S2;
        } else {
            state_ = State::S3;
        }
        break;
    }

    case State::S3:
        // Release DB (was holding upper address for U18 latch in S2).
        release_data();
        // Datasheet p.4: "wait states (SW) can be inserted between
        // S2 or S3 and S4 by the use of the Ready line."
        if (pin_ready_.level() != Level::High) break;  // Sw
        state_ = State::S4;
        break;

    case State::S4: {
        auto& ch = ch_[active_ch_];

        spdlog::trace("[8237A] S4 entry ch{}: addr={:#06x} count={} HLDA={}",
                      active_ch_, ch.current_address, ch.current_count,
                      int(pin_hlda_.level()));

        // Deassert all strobes
        pin_memr_.drive(Level::High);
        pin_memw_.drive(Level::High);
        pin_ior_.drive(Level::High);
        pin_iow_.drive(Level::High);

        // Memory-to-memory read phase complete: capture data, switch to write phase
        if ((command_ & 0x01) && !mem2mem_write_) {
            temp_ = read_data();
            spdlog::debug("[8237A] S4 ch0 m2m read: temp=0x{:02X}, switching to ch1 write", temp_);

            // Update ch0 address (source)
            bool decrement = (ch.mode & 0x20) != 0;
            if (decrement) ch.current_address--;
            else           ch.current_address++;

            // If ch0 address hold (command bit 1), restore address
            if (command_ & 0x02)
                ch.current_address = ch.base_address;

            // Switch to ch1 for write phase
            active_ch_ = 1;
            mem2mem_write_ = true;
            state_ = State::M2M_S1;
            break;
        }

        // Update address
        bool decrement = (ch.mode & 0x20) != 0;
        if (decrement)
            ch.current_address--;
        else
            ch.current_address++;

        // Terminal count check (ch1 for m2m, active_ch_ otherwise)
        bool tc = false;
        spdlog::debug("[8237A] S4 ch{}: count={} addr={:#06x}", active_ch_, ch.current_count, ch.current_address);
        if (ch.current_count == 0) {
            tc = true;
            ch.tc_reached = true;

            pin_eop_.drive(Level::Low);
            eop_pending_ = true;
            spdlog::debug("[8237A] TC! ch{} ~EOP driven Low", active_ch_);

            if (ch.mode & 0x10) {
                ch.current_address = ch.base_address;
                ch.current_count = ch.base_count;
            } else {
                ch.masked = true;
            }
        } else {
            ch.current_count--;
        }

        // Memory-to-memory write phase complete: loop back to ch0 read
        if (mem2mem_write_) {
            mem2mem_write_ = false;
            if (tc) {
                end_dma_service();
            } else {
                // Continue: switch back to ch0 for next read
                active_ch_ = 0;
                state_ = State::S1;
            }
            break;
        }

        // Normal DMA: determine whether to release the bus or continue
        uint8_t mode_type = (ch.mode >> 6) & 0x03;
        bool release_bus = false;

        if (mode_type == 0x01) {
            release_bus = true;
        } else if (mode_type == 0x00) {
            release_bus = tc || (pin_dreq_[active_ch_].level() != Level::High);
        } else if (mode_type == 0x02) {
            release_bus = tc;
        } else {
            release_bus = tc;
        }

        spdlog::trace("[8237A] S4 ch{}: mode_type={} release_bus={} tc={}", active_ch_, mode_type, release_bus, tc);
        if (release_bus) {
            end_dma_service();
        } else {
            uint8_t new_upper = static_cast<uint8_t>(ch.current_address >> 8);
            if (new_upper != prev_upper_addr_) {
                state_ = State::S1;
            } else {
                for (int i = 0; i < 8; ++i)
                    pin_a_[i].drive((ch.current_address >> i) & 1 ? Level::High : Level::Low);
                state_ = State::S2;
            }
        }
        break;
    }

    // =====================================================================
    // Memory-to-memory write phase (ch1 destination)
    // =====================================================================
    case State::M2M_S1: {
        auto& ch = ch_[1];

        // Drive ch1 address (destination)
        for (int i = 0; i < 8; ++i)
            pin_a_[i].drive((ch.current_address >> i) & 1 ? Level::High : Level::Low);
        a_driving_ = true;

        uint8_t upper = static_cast<uint8_t>(ch.current_address >> 8);
        drive_data(upper);
        prev_upper_addr_ = upper;

        pin_adstb_.drive(Level::High);

        // M2M write: 8237A DB -> XD -> U13(XD->D) -> D -> U12(D->MD) -> DRAM
        xcvr_m_->set_driving(IC_74S245::Driving::B);  // D->MD
        xcvr_x_->set_driving(IC_74S245::Driving::A);  // XD->D

        spdlog::debug("[8237A] M2M_S1 ch1: addr={:#06x} (write temp=0x{:02X})",
                      ch.current_address, temp_);
        state_ = State::M2M_S2;
        break;
    }

    case State::M2M_S2: {
        pin_adstb_.drive(Level::Low);

        // Drive temp register data onto bus
        drive_data(temp_);

        // Assert ~MEMW
        pin_memw_.drive(Level::Low);

        xcvr_c_->set_driving(IC_74S245::Driving::A);

        spdlog::debug("[8237A] M2M_S2 ch1: data=0x{:02X}", temp_);

        bool compressed = (command_ & 0x08) != 0;
        state_ = compressed ? State::M2M_S4 : State::M2M_S3;
        break;
    }

    case State::M2M_S3:
        state_ = State::M2M_S4;
        break;

    case State::M2M_S4:
        // Deassert strobes, release data, fall through to S4 for count/TC
        pin_memw_.drive(Level::High);
        release_data();
        state_ = State::S4;  // S4 handles count, TC, and looping back to ch0
        break;

    } // switch
}

void IC_8237A::end_dma_service() {
    int ch_idx = active_ch_;

    // Deassert ~DACK
    if (ch_idx >= 0)
        pin_dack_[ch_idx].drive(Level::High);

    // Release bus: HRQ and AEN drop. HOLDA clears async (U67 ~CLR).
    // AEN_BRD drops on next cycle (U98 latches HOLDA=Low).
    pin_hrq_.drive(Level::Low); hrq_driven_ = false;
    pin_aen_.drive(Level::Low);

    // Release address and data pins
    release_address();
    release_data();

    // Clear software request for the channel that just completed.
    if (ch_idx >= 0)
        ch_[ch_idx].request = false;
    if (command_ & 0x01)
        ch_[0].request = false;

    state_ = State::SI;
    active_ch_ = -1;
    mem2mem_write_ = false;

    spdlog::debug("[8237A] DMA service complete for ch{}", ch_idx);

    // Bus released. HLDA will clear via U52/U67 handshake.
    // evaluate_dreq() won't re-request until HLDA is Low.
}

void IC_8237A::drive_data(uint8_t value) {
    for (int i = 0; i < 8; ++i) {
        pin_db_[i].drive((value >> i) & 1 ? Level::High : Level::Low);
    }
    db_driving_ = true;
}

void IC_8237A::release_data() {
    if (!db_driving_) return;
    for (int i = 0; i < 8; ++i) {
        pin_db_[i].release();
    }
    db_driving_ = false;
}

void IC_8237A::release_address() {
    if (!a_driving_) return;
    for (int i = 0; i < 8; ++i)
        pin_a_[i].release();
    a_driving_ = false;
}

uint8_t IC_8237A::read_data() const {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_db_[i].level() == Level::High)
            val |= (1 << i);
    }
    return val;
}

} // namespace bench
