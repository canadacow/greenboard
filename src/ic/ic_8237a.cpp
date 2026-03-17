#include "ic/ic_8237a.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8237A::IC_8237A() : CallbackComponent("8237A") { set_description("DMA"); }

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
    declare_async_input(pin_cs_);    // ~CS: CPU programming only, never during DMA transfers
    declare_async_input(pin_clk_);   // DCLK: clock input, no combinational dependency
    declare_input(pin_reset_);
    declare_async_input(pin_hlda_);  // HLDA: latched by external FFs, cross-cycle
    for (int i = 0; i < 4; ++i) declare_async_input(pin_dreq_[i]);  // async DMA requests
    declare_output(pin_hrq_); declare_output(pin_eop_);
    for (int i = 0; i < 4; ++i) declare_output(pin_dack_[i]);
    declare_output(pin_adstb_); declare_output(pin_aen_);

    // Address pins: input during CPU I/O (register select), output only during
    // DMA transfer.  Declaring as bidir avoids a DAG cycle (8237A -> XA -> U66
    // -> ~DMA_CS -> 8237A).  Direction starts as Input since DMA is idle.
    for (int i = 0; i < 8; ++i) declare_input(pin_a_[i]);
    declare_bidir_block({pin_a_[0], pin_a_[1], pin_a_[2], pin_a_[3],
                         pin_a_[4], pin_a_[5], pin_a_[6], pin_a_[7]},
        BidirDir::Input | BidirDir::Output,
        [this]() { return is_dma_active() ? BidirDir::Output : BidirDir::Input; });

    // Data bus: output during CPU reads of DMA registers (~CS+~IOR active),
    // input otherwise.  Bidir avoids cycle through 8237A -> D -> U8/U12 -> D -> 8237A.
    declare_bidir_block({pin_db_[0], pin_db_[1], pin_db_[2], pin_db_[3],
                         pin_db_[4], pin_db_[5], pin_db_[6], pin_db_[7]},
        BidirDir::Input | BidirDir::Output,
        [this]() {
            // Output when: CPU reading DMA regs, or S1 (upper addr on DB)
            if (state_ == State::S1) return BidirDir::Output;
            return (pin_cs_.level() == Level::Low && pin_ior_.level() == Level::Low)
                ? BidirDir::Output : BidirDir::Input;
        });

    // ~MEMR/~MEMW: output during DMA transfers, HiZ otherwise.
    // HiZ in CPU mode removes false DAG edges U14->U35 via command strobes.
    declare_input(pin_memr_); declare_input(pin_memw_);
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

    // Bus write: ~IOW falling while ~CS active
    if (iow_cur == Level::Low && iow_prev_ != Level::Low && cs_cur == Level::Low)
        on_bus_write();
    if (cs_cur == Level::Low && cs_prev_ != Level::Low && iow_cur == Level::Low)
        on_bus_write();

    // Bus read: ~IOR falling while ~CS active
    if (ior_cur == Level::Low && ior_prev_ != Level::Low && cs_cur == Level::Low)
        on_bus_read();
    if (cs_cur == Level::Low && cs_prev_ != Level::Low && ior_cur == Level::Low)
        on_bus_read();

    // Release data bus when ~IOR or ~CS goes inactive (was active Low, now not)
    if ((ior_cur != Level::Low && ior_prev_ == Level::Low) ||
        (cs_cur != Level::Low && cs_prev_ == Level::Low))
        release_data();

    // DREQ changes -- check for new DMA requests
    evaluate_dreq();

    // CLK falling edge -- advance DMA state machine
    if (clk_cur == Level::Low && clk_prev_ == Level::High)
        on_clk_falling();

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
    pin_hrq_.drive(Level::Low);
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

    // Fixed priority: CH0 highest
    for (int i = 0; i < 4; ++i) {
        if (ch_[i].masked) continue;
        bool dreq = (pin_dreq_[i].level() == Level::High) || ch_[i].request;
        if (dreq) {
            // Assert HRQ, wait for HLDA
            active_ch_ = i;
            state_ = State::BusRequested;
            pin_hrq_.drive(Level::High);
            spdlog::debug("[8237A] HRQ asserted for ch{}", i);
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
        // Nothing -- evaluate_dreq() handles transition to BusRequested
        break;

    case State::BusRequested:
        // HLDA is sampled at CLK falling edge
        if (pin_hlda_.level() == Level::High) {
            spdlog::debug("[8237A] HLDA received, entering S1 for ch{}", active_ch_);
            state_ = State::S1;
        } else {
            break;
        }
        // Fall through to execute S1 in this same CLK cycle
        [[fallthrough]];

    case State::S1: {
        auto& ch = ch_[active_ch_];

        // AEN high -- signals external logic that DMA owns the bus
        pin_aen_.drive(Level::High);

        // Assert ~DACK for active channel
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

        state_ = State::S2;
        break;
    }

    case State::S2: {
        // ADSTB falling edge latches upper address into external 74S373 (U18)
        pin_adstb_.drive(Level::Low);

        // Release data bus (was holding upper address)
        release_data();

        // Assert memory strobes based on transfer type
        auto& ch = ch_[active_ch_];
        uint8_t transfer_type = (ch.mode >> 2) & 0x03;
        // 00=verify, 01=write (IO->mem), 10=read (mem->IO), 11=illegal
        if (transfer_type == 0x01) {
            pin_memw_.drive(Level::Low);   // IO -> memory write
        } else if (transfer_type == 0x02) {
            pin_memr_.drive(Level::Low);   // memory read -> IO
        }
        // verify (00): no strobes, address still generated

        // Compressed timing: skip S3 (command register bit 0)
        bool compressed = (command_ & 0x01) != 0;
        state_ = compressed ? State::S4 : State::S3;
        break;
    }

    case State::S3:
        // Wait state -- extends read access time.
        // TODO: check READY for wait-state insertion
        state_ = State::S4;
        break;

    case State::S4: {
        auto& ch = ch_[active_ch_];

        // Deassert memory strobes
        pin_memr_.drive(Level::High);
        pin_memw_.drive(Level::High);

        // Update address
        bool decrement = (ch.mode & 0x20) != 0;
        if (decrement)
            ch.current_address--;
        else
            ch.current_address++;

        // Terminal count check
        bool tc = false;
        if (ch.current_count == 0) {
            tc = true;
            ch.tc_reached = true;

            // Assert ~EOP (active low) -- will be deasserted next CLK falling
            pin_eop_.drive(Level::Low);
            eop_pending_ = true;

            // Auto-initialize: reload base values
            if (ch.mode & 0x10) {
                ch.current_address = ch.base_address;
                ch.current_count = ch.base_count;
            } else {
                ch.masked = true;  // mask channel after TC
            }
        } else {
            ch.current_count--;
        }

        // Determine whether to release the bus or continue
        uint8_t mode_type = (ch.mode >> 6) & 0x03;
        // 00=demand, 01=single, 10=block, 11=cascade
        bool release_bus = false;

        if (mode_type == 0x01) {
            // Single transfer: always release after each byte
            release_bus = true;
        } else if (mode_type == 0x00) {
            // Demand: release if DREQ inactive or TC
            release_bus = tc || (pin_dreq_[active_ch_].level() != Level::High);
        } else if (mode_type == 0x02) {
            // Block: release only on TC/EOP
            release_bus = tc;
        } else {
            // Cascade: release on TC
            release_bus = tc;
        }

        if (release_bus) {
            end_dma_service();
        } else {
            // Continue with next transfer.
            // Optimization: skip S1 if upper address byte hasn't changed
            // (datasheet: "S1 states only when updating of A8-A15 is necessary")
            uint8_t new_upper = static_cast<uint8_t>(ch.current_address >> 8);
            if (new_upper != prev_upper_addr_) {
                state_ = State::S1;  // need to re-latch upper address
            } else {
                // Drive updated A0-A7 directly, skip S1
                for (int i = 0; i < 8; ++i)
                    pin_a_[i].drive((ch.current_address >> i) & 1 ? Level::High : Level::Low);
                // Re-assert strobes for next transfer
                uint8_t tt = (ch.mode >> 2) & 0x03;
                if (tt == 0x01)
                    pin_memw_.drive(Level::Low);
                else if (tt == 0x02)
                    pin_memr_.drive(Level::Low);
                // Go to S3 or S4 (compressed skips S3)
                bool compressed = (command_ & 0x01) != 0;
                state_ = compressed ? State::S4 : State::S3;
            }
        }
        break;
    }

    } // switch
}

void IC_8237A::end_dma_service() {
    int ch_idx = active_ch_;

    // Deassert ~DACK
    if (ch_idx >= 0)
        pin_dack_[ch_idx].drive(Level::High);

    // Release bus
    pin_hrq_.drive(Level::Low);
    pin_aen_.drive(Level::Low);

    // Release address and data pins
    release_address();
    release_data();

    // Clear software request for the channel that just completed
    if (ch_idx >= 0)
        ch_[ch_idx].request = false;

    state_ = State::SI;
    active_ch_ = -1;

    spdlog::debug("[8237A] DMA service complete for ch{}", ch_idx);

    // Check for more pending requests
    evaluate_dreq();
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
