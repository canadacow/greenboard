#include "ic/ic_8237a.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8237A::IC_8237A() : CallbackComponent("8237A") {}

void IC_8237A::install(Socket& socket) {
    // Data bus: DB0=pin30 .. DB5=pin23 (skipping pin24,25=DACK), DB4=pin26, DB7=pin21
    pin_db_[0] = socket.pin_signal(30);
    pin_db_[1] = socket.pin_signal(29);
    pin_db_[2] = socket.pin_signal(28);
    pin_db_[3] = socket.pin_signal(27);
    pin_db_[4] = socket.pin_signal(26);
    pin_db_[5] = socket.pin_signal(23);
    pin_db_[6] = socket.pin_signal(22);
    pin_db_[7] = socket.pin_signal(21);

    // Address pins: A0-A3 = pin32-35, A4-A7 = pin37-40
    pin_a_[0] = socket.pin_signal(32);
    pin_a_[1] = socket.pin_signal(33);
    pin_a_[2] = socket.pin_signal(34);
    pin_a_[3] = socket.pin_signal(35);
    pin_a_[4] = socket.pin_signal(37);
    pin_a_[5] = socket.pin_signal(38);
    pin_a_[6] = socket.pin_signal(39);
    pin_a_[7] = socket.pin_signal(40);

    // Control
    pin_ior_   = socket.pin_signal(1);
    pin_iow_   = socket.pin_signal(2);
    pin_cs_    = socket.pin_signal(11);
    pin_clk_   = socket.pin_signal(12);
    pin_reset_ = socket.pin_signal(13);
    pin_ready_ = socket.pin_signal(6);
    pin_hlda_  = socket.pin_signal(7);
    pin_eop_   = socket.pin_signal(36);
    pin_vcc_   = socket.pin_signal(5);

    // DREQ inputs: DREQ0=pin19, DREQ1=pin18, DREQ2=pin17, DREQ3=pin16
    pin_dreq_[0] = socket.pin_signal(19);
    pin_dreq_[1] = socket.pin_signal(18);
    pin_dreq_[2] = socket.pin_signal(17);
    pin_dreq_[3] = socket.pin_signal(16);

    // Outputs
    pin_hrq_   = socket.pin_signal(10);
    pin_dack_[0] = socket.pin_signal(25);  // ~DACK0
    pin_dack_[1] = socket.pin_signal(24);  // ~DACK1
    pin_dack_[2] = socket.pin_signal(14);  // ~DACK2
    pin_dack_[3] = socket.pin_signal(15);  // ~DACK3
    pin_memr_  = socket.pin_signal(3);
    pin_memw_  = socket.pin_signal(4);
    pin_adstb_ = socket.pin_signal(8);
    pin_aen_   = socket.pin_signal(9);

    // Subscribe to inputs
    if (pin_cs_)    pin_cs_->connect(this);
    if (pin_ior_)   pin_ior_->connect(this);
    if (pin_iow_)   pin_iow_->connect(this);
    if (pin_clk_)   pin_clk_->connect(this);
    if (pin_reset_) pin_reset_->connect(this);
    if (pin_hlda_)  pin_hlda_->connect(this);
    if (pin_vcc_)   pin_vcc_->connect(this);
    for (int i = 0; i < 4; ++i) {
        if (pin_dreq_[i]) pin_dreq_[i]->connect(this);
    }


void IC_8237A::on_signal_change() {
    Level reset_cur = pin_reset_ ? pin_reset_->level() : Level::HiZ;
    Level iow_cur = pin_iow_ ? pin_iow_->level() : Level::HiZ;
    Level cs_cur = pin_cs_ ? pin_cs_->level() : Level::HiZ;
    Level ior_cur = pin_ior_ ? pin_ior_->level() : Level::HiZ;
    Level clk_cur = pin_clk_ ? pin_clk_->level() : Level::HiZ;
    Level hlda_cur = pin_hlda_ ? pin_hlda_->level() : Level::HiZ;

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

    // Release data bus when ~IOR or ~CS goes inactive
    if ((ior_cur == Level::High && ior_prev_ != Level::High) ||
        (cs_cur == Level::High && cs_prev_ != Level::High))
        release_data();

    // DREQ changes -- check for new DMA requests
    evaluate_dreq();

    // CLK falling edge -- advance DMA state machine
    if (clk_cur == Level::Low && clk_prev_ == Level::High)
        on_clk_falling();

    // HLDA rising -- CPU has released the bus
    if (hlda_cur == Level::High && hlda_prev_ != Level::High) {
        if (state_ == State::RequestPending)
            state_ = State::Transfer;
    }

    reset_prev_ = reset_cur;
    iow_prev_ = iow_cur;
    cs_prev_ = cs_cur;
    ior_prev_ = ior_cur;
    clk_prev_ = clk_cur;
    hlda_prev_ = hlda_cur;
}

void IC_8237A::on_reset() {
    command_ = 0;
    status_ = 0;
    temp_ = 0;
    flip_flop_ = false;
    disabled_ = false;
    state_ = State::Idle;
    active_ch_ = -1;

    for (int i = 0; i < 4; ++i) {
        ch_[i].masked = true;
        ch_[i].request = false;
        ch_[i].tc_reached = false;
    }

    // Deassert outputs
    if (pin_hrq_) pin_hrq_->drive(Level::Low);
    for (int i = 0; i < 4; ++i) {
        if (pin_dack_[i]) pin_dack_[i]->drive(Level::High);  // active low
    }
    if (pin_aen_)   pin_aen_->drive(Level::Low);
    if (pin_adstb_) pin_adstb_->drive(Level::Low);

    spdlog::debug("[8237A] reset");
}

void IC_8237A::on_bus_write() {
    uint8_t data = read_data();
    // Read A0-A3 for register select
    uint8_t reg = 0;
    for (int i = 0; i < 4; ++i) {
        if (pin_a_[i] && pin_a_[i]->level() == Level::High)
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
        if (pin_a_[i] && pin_a_[i]->level() == Level::High)
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
                if (pin_dreq_[i] && pin_dreq_[i]->level() == Level::High)
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
    if (disabled_ || state_ != State::Idle) return;

    // Fixed priority: CH0 highest
    for (int i = 0; i < 4; ++i) {
        if (ch_[i].masked) continue;
        bool dreq = (pin_dreq_[i] && pin_dreq_[i]->level() == Level::High) || ch_[i].request;
        if (dreq) {
            // Assert HRQ, wait for HLDA
            active_ch_ = i;
            state_ = State::RequestPending;
            if (pin_hrq_) pin_hrq_->drive(Level::High);
            return;
        }
    }
}

void IC_8237A::on_clk_falling() {
    if (state_ != State::Transfer || active_ch_ < 0) return;

    auto& ch = ch_[active_ch_];

    // Assert ~DACK for active channel
    if (pin_dack_[active_ch_])
        pin_dack_[active_ch_]->drive(Level::Low);  // active low

    // Enable address bus and strobe
    if (pin_aen_) pin_aen_->drive(Level::High);

    // Drive address on A0-A7 (lower 8 bits of current address)
    for (int i = 0; i < 8; ++i) {
        if (pin_a_[i])
            pin_a_[i]->drive((ch.current_address >> i) & 1 ? Level::High : Level::Low);
    }

    // Strobe upper address onto data bus (for 74LS373 latch)
    if (pin_adstb_) pin_adstb_->drive(Level::High);
    for (int i = 0; i < 8; ++i) {
        if (pin_db_[i])
            pin_db_[i]->drive((ch.current_address >> (i + 8)) & 1 ? Level::High : Level::Low);
    }
    if (pin_adstb_) pin_adstb_->drive(Level::Low);

    // Drive appropriate memory strobe based on transfer type
    uint8_t transfer_type = (ch.mode >> 2) & 0x03;
    // 00=verify, 01=write (IO->mem), 10=read (mem->IO)
    if (transfer_type == 0x01) {
        if (pin_memw_) pin_memw_->drive(Level::Low);
    } else if (transfer_type == 0x02) {
        if (pin_memr_) pin_memr_->drive(Level::Low);
    }

    // Update address and count
    bool decrement = (ch.mode & 0x20) != 0;
    if (decrement)
        ch.current_address--;
    else
        ch.current_address++;

    // Terminal count check
    if (ch.current_count == 0) {
        ch.tc_reached = true;

        // Pulse ~EOP
        if (pin_eop_) {
            pin_eop_->drive(Level::Low);
            pin_eop_->drive(Level::High);
        }

        // Auto-initialize: reload base values
        bool auto_init = (ch.mode & 0x10) != 0;
        if (auto_init) {
            ch.current_address = ch.base_address;
            ch.current_count = ch.base_count;
        } else {
            ch.masked = true;  // mask channel after TC
        }
    } else {
        ch.current_count--;
    }

    // Deassert memory strobes
    if (pin_memr_) pin_memr_->drive(Level::High);
    if (pin_memw_) pin_memw_->drive(Level::High);

    // Deassert DACK
    if (pin_dack_[active_ch_])
        pin_dack_[active_ch_]->drive(Level::High);

    // For single transfer mode, release bus after each byte
    uint8_t mode_type = (ch.mode >> 6) & 0x03;
    // 00=demand, 01=single, 10=block, 11=cascade
    if (mode_type == 0x01 || ch.tc_reached) {
        // Release bus
        if (pin_hrq_) pin_hrq_->drive(Level::Low);
        if (pin_aen_) pin_aen_->drive(Level::Low);

        // Release address pins
        for (int i = 0; i < 8; ++i) {
            if (pin_a_[i]) pin_a_[i]->release();
        }
        release_data();

        state_ = State::Idle;
        active_ch_ = -1;
        ch_[active_ch_ >= 0 ? active_ch_ : 0].request = false;

        // Check for more pending requests
        evaluate_dreq();
    }
}

void IC_8237A::drive_data(uint8_t value) {
    for (int i = 0; i < 8; ++i) {
        if (pin_db_[i])
            pin_db_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
    }
}

void IC_8237A::release_data() {
    for (int i = 0; i < 8; ++i) {
        if (pin_db_[i])
            pin_db_[i]->release();
    }
}

uint8_t IC_8237A::read_data() const {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_db_[i] && pin_db_[i]->level() == Level::High)
            val |= (1 << i);
    }
    return val;
}

} // namespace bench
