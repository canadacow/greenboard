#include "ic/ic_8259a.h"
#include "core/inline_component.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8259A::IC_8259A() : ThreadedComponent("8259A") {}

void IC_8259A::install(Socket& socket) {
    // Data bus: D7=pin4, D6=pin5, ..., D0=pin11
    for (int i = 0; i < 8; ++i)
        pin_d_[i] = socket.pin_signal(11 - i);  // D0=pin11, D1=pin10, ..., D7=pin4

    // Control inputs
    pin_cs_   = socket.pin_signal(1);   // ~CS
    pin_wr_   = socket.pin_signal(2);   // ~WR
    pin_rd_   = socket.pin_signal(3);   // ~RD
    pin_spen_ = socket.pin_signal(16);  // ~SP/~EN
    pin_inta_ = socket.pin_signal(26);  // ~INTA
    pin_a0_   = socket.pin_signal(27);  // A0
    pin_vcc_  = socket.pin_signal(28);  // VCC

    // Interrupt request inputs
    for (int i = 0; i < 8; ++i)
        pin_ir_[i] = socket.pin_signal(18 + i);  // IR0=pin18 .. IR7=pin25

    // Interrupt output
    pin_int_ = socket.pin_signal(17);   // INT

    // Subscribe to control signals
    if (pin_cs_)   pin_cs_->connect(this);
    if (pin_wr_)   pin_wr_->connect(this);
    if (pin_rd_)   pin_rd_->connect(this);
    if (pin_inta_) pin_inta_->connect(this);
    if (pin_vcc_)  pin_vcc_->connect(this);

    // Subscribe to all IR lines for edge detection
    for (int i = 0; i < 8; ++i) {
        if (pin_ir_[i]) pin_ir_[i]->connect(this);
    }

    spdlog::debug("[8259A] installed into socket {}", socket.ref());
}

void IC_8259A::on_power_on() {
    irr_ = 0;
    isr_ = 0;
    imr_ = 0;
    vector_base_ = 0;
    icw1_ = 0;
    icw4_needed_ = false;
    single_mode_ = true;
    edge_triggered_ = true;
    auto_eoi_ = false;
    mode_8086_ = true;
    ir_prev_ = 0;
    read_isr_ = false;
    inta_count_ = 0;
    inta_level_ = -1;
    init_state_ = InitState::Ready;
    initialized_ = false;
    wr_prev_ = Level::HiZ;
    cs_prev_ = Level::HiZ;
    rd_prev_ = Level::HiZ;
    inta_prev_ = Level::HiZ;
}

void little_sleep(std::chrono::microseconds us)
{
    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + us;
    do
    {
        std::this_thread::yield();
    }
    while (std::chrono::high_resolution_clock::now() < end);
}

void IC_8259A::on_signal_change() {
    Level wr_cur = pin_wr_ ? pin_wr_->level() : Level::HiZ;
    Level cs_cur = pin_cs_ ? pin_cs_->level() : Level::HiZ;
    Level rd_cur = pin_rd_ ? pin_rd_->level() : Level::HiZ;
    Level inta_cur = pin_inta_ ? pin_inta_->level() : Level::HiZ;

    auto d_before = read_data();
    bool a0_before = pin_a0_ && pin_a0_->level() == Level::High;    

    //little_sleep(std::chrono::microseconds(100));

    //auto d_after = read_data();
    //bool a0_after = pin_a0_ && pin_a0_->level() == Level::High;
    //spdlog::info("[8259A] D:{:#04x}->{:#04x} A0:{}->{}", d_before, d_after, a0_before, a0_after);    

    // Bus write: ~WR falling while ~CS active
    if (wr_cur == Level::Low && wr_prev_ != Level::Low && cs_cur == Level::Low)
        on_bus_write();
    // Bus write: ~CS falling while ~WR active
    if (cs_cur == Level::Low && cs_prev_ != Level::Low && wr_cur == Level::Low)
        on_bus_write();

    // Bus read: ~RD falling while ~CS active
    if (rd_cur == Level::Low && rd_prev_ != Level::Low && cs_cur == Level::Low)
        on_bus_read();
    // Bus read: ~CS falling while ~RD active
    if (cs_cur == Level::Low && cs_prev_ != Level::Low && rd_cur == Level::Low)
        on_bus_read();

    // Release data bus when ~RD or ~CS goes inactive
    if ((rd_cur == Level::High && rd_prev_ != Level::High) ||
        (cs_cur == Level::High && cs_prev_ != Level::High))
        release_data();

    // ~INTA falling edge
    if (inta_cur == Level::Low && inta_prev_ != Level::Low)
        on_inta_falling();

    // ~INTA rising edge: release data bus after second pulse
    if (inta_cur == Level::High && inta_prev_ != Level::High && inta_count_ >= 2) {
        release_data();
        inta_count_ = 0;
        inta_level_ = -1;
    }

    wr_prev_ = wr_cur;
    cs_prev_ = cs_cur;
    rd_prev_ = rd_cur;
    inta_prev_ = inta_cur;

    // IRQ line changes -- edge detection
    for (int i = 0; i < 8; ++i) {
        if (!pin_ir_[i]) continue;
        bool now_high = pin_ir_[i]->level() == Level::High;
        bool was_low = (ir_prev_ & (1 << i)) == 0;
        if (now_high && was_low) {
            ir_prev_ |= (1 << i);
            if (edge_triggered_) {
                irr_ |= (1 << i);
                evaluate_int();
            }
        } else if (!now_high && !was_low) {
            ir_prev_ &= ~(1 << i);
            if (!edge_triggered_) {
                irr_ &= ~(1 << i);
                evaluate_int();
            }
        }
    }
}

void IC_8259A::on_bus_write() {
    uint8_t data = read_data();
    bool a0 = pin_a0_ && pin_a0_->level() == Level::High;

    if (!a0 && (data & 0x10)) {
        // ICW1: A0=0, D4=1
        icw1_ = data;
        edge_triggered_ = !(data & 0x08);   // LTIM: 0=edge, 1=level
        single_mode_    =  (data & 0x02);    // SNGL: 1=single
        icw4_needed_    =  (data & 0x01);    // IC4:  1=ICW4 needed
        init_state_ = InitState::WaitICW2;
        initialized_ = false;

        // Reset internal state
        imr_ = 0;
        isr_ = 0;
        irr_ = 0;
        auto_eoi_ = false;
        read_isr_ = false;
        inta_count_ = 0;
        inta_level_ = -1;

        // Deassert INT during initialization
        if (pin_int_) pin_int_->drive(Level::Low);

        spdlog::trace("[8259A] ICW1={:#04x} edge={} single={} icw4={}",
                      data, edge_triggered_, single_mode_, icw4_needed_);
        return;
    }

    // Initialization sequence
    if (init_state_ == InitState::WaitICW2 && a0) {
        vector_base_ = data & 0xF8;  // upper 5 bits = base vector
        spdlog::trace("[8259A] ICW2={:#04x} vector_base={:#04x}", data, vector_base_);
        if (!single_mode_)
            init_state_ = InitState::WaitICW3;
        else if (icw4_needed_)
            init_state_ = InitState::WaitICW4;
        else {
            init_state_ = InitState::Ready;
            initialized_ = true;
            spdlog::trace("[8259A] initialized (no ICW3/ICW4)");
            evaluate_int();
        }
        return;
    }

    if (init_state_ == InitState::WaitICW3 && a0) {
        // ICW3: cascade info -- ignored on 5150 (single mode)
        spdlog::trace("[8259A] ICW3={:#04x} (ignored, single mode)", data);
        if (icw4_needed_)
            init_state_ = InitState::WaitICW4;
        else {
            init_state_ = InitState::Ready;
            initialized_ = true;
            evaluate_int();
        }
        return;
    }

    if (init_state_ == InitState::WaitICW4 && a0) {
        mode_8086_ = (data & 0x01);    // uPM: 1=8086, 0=8080
        auto_eoi_  = (data & 0x02);    // AEOI: 1=auto EOI
        spdlog::trace("[8259A] ICW4={:#04x} mode_8086={} auto_eoi={}", data, mode_8086_, auto_eoi_);
        init_state_ = InitState::Ready;
        initialized_ = true;

        // Scan for any pending IRQs
        for (int i = 0; i < 8; ++i) {
            if (pin_ir_[i] && pin_ir_[i]->level() == Level::High) {
                ir_prev_ |= (1 << i);
                if (edge_triggered_) {
                    // In edge mode after init, treat current high lines as pending
                    // (the BIOS expects IRQ0 from PIT to be noticed)
                    irr_ |= (1 << i);
                }
            }
        }
        evaluate_int();
        return;
    }

    // Operational command words (after initialization)
    if (!initialized_) return;

    if (a0) {
        // OCW1: A0=1, write IMR
        imr_ = data;
        spdlog::trace("[8259A] OCW1 IMR={:#04x}", imr_);
        evaluate_int();
        return;
    }

    // A0=0: OCW2 or OCW3
    if ((data & 0x18) == 0x00) {
        // OCW2: D4=0, D3=0
        int eoi_type = (data >> 5) & 0x07;
        switch (eoi_type) {
            case 0x01: {
                // Non-specific EOI: clear highest-priority ISR bit
                int lvl = highest_priority_irq(isr_);
                if (lvl >= 0) {
                    isr_ &= ~(1 << lvl);
                    spdlog::trace("[8259A] non-specific EOI, cleared ISR bit {}", lvl);
                }
                break;
            }
            case 0x03: {
                // Specific EOI: clear ISR bit specified in L2-L0
                int lvl = data & 0x07;
                isr_ &= ~(1 << lvl);
                spdlog::trace("[8259A] specific EOI, cleared ISR bit {}", lvl);
                break;
            }
            case 0x05: {
                // Rotate on non-specific EOI
                int lvl = highest_priority_irq(isr_);
                if (lvl >= 0)
                    isr_ &= ~(1 << lvl);
                // Rotation not implemented (5150 BIOS doesn't use it)
                break;
            }
            default:
                break;
        }
        evaluate_int();
    } else if ((data & 0x18) == 0x08) {
        // OCW3: D4=0, D3=1
        if (data & 0x02) {
            read_isr_ = (data & 0x01);
        }
    }
}

void IC_8259A::on_bus_read() {
    if (!initialized_) return;
    bool a0 = pin_a0_ && pin_a0_->level() == Level::High;

    if (a0) {
        // A0=1: read IMR
        drive_data(imr_);
    } else {
        // A0=0: read IRR or ISR (selected by OCW3)
        drive_data(read_isr_ ? isr_ : irr_);
    }
}

void IC_8259A::on_inta_falling() {
    if (!initialized_) return;

    inta_count_++;

    if (inta_count_ == 1) {
        // First INTA pulse: freeze priority, set ISR, clear IRR
        inta_level_ = highest_priority_irq(irr_ & ~imr_);
        if (inta_level_ >= 0) {
            isr_ |= (1 << inta_level_);
            irr_ &= ~(1 << inta_level_);
            // Deassert INT
            if (pin_int_) pin_int_->drive(Level::Low);
        }
    } else if (inta_count_ == 2) {
        // Second INTA pulse: put vector on data bus
        if (inta_level_ >= 0) {
            uint8_t vector;
            if (mode_8086_) {
                vector = vector_base_ | inta_level_;
            } else {
                // 8080 mode: not used on 5150
                vector = vector_base_ | (inta_level_ << 2);
            }
            drive_data(vector);

            // Auto-EOI: clear ISR bit immediately
            if (auto_eoi_) {
                isr_ &= ~(1 << inta_level_);
            }
        }
        evaluate_int();
    }
}

void IC_8259A::evaluate_int() {
    if (!initialized_) return;

    // Find highest-priority unmasked request
    uint8_t pending = irr_ & ~imr_;
    int req = highest_priority_irq(pending);

    if (req >= 0) {
        // Check if this request has higher priority than what's in service
        int svc = highest_priority_irq(isr_);
        if (svc < 0 || req < svc) {
            // Higher priority (lower number) -- assert INT
            if (pin_int_) pin_int_->drive(Level::High);
            return;
        }
    }

    // No pending interrupt or blocked by ISR -- deassert INT
    if (pin_int_) pin_int_->drive(Level::Low);
}

void IC_8259A::drive_data(uint8_t value) {
    auto& ic = pin_d_[0]->get_inline();
    ic.begin_transaction();
    for (int i = 0; i < 8; ++i) {
        if (pin_d_[i])
            pin_d_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
    }
    ic.commit_transaction();
}

void IC_8259A::release_data() {
    auto& ic = pin_d_[0]->get_inline();
    ic.begin_transaction();
    for (int i = 0; i < 8; ++i) {
        if (pin_d_[i])
            pin_d_[i]->release();
    }
    ic.commit_transaction();
}

uint8_t IC_8259A::read_data() const {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_d_[i] && pin_d_[i]->level() == Level::High)
            val |= (1 << i);
    }
    return val;
}

int IC_8259A::highest_priority_irq(uint8_t reg) const {
    // Fixed priority: IR0 = highest, IR7 = lowest
    for (int i = 0; i < 8; ++i) {
        if (reg & (1 << i))
            return i;
    }
    return -1;
}

} // namespace bench
