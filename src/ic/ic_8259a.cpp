#include "ic/ic_8259a.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8259A::IC_8259A() : CallbackComponent("8259A") { set_description("PIC"); }

void IC_8259A::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    // Data bus: D0=pin11, D1=pin10, ..., D7=pin4
    for (int i = 0; i < 8; ++i)
        d_[i] = pin(11 - i);

    // Control inputs
    cs_   = connect_pin(1);
    wr_   = connect_pin(2);
    rd_   = connect_pin(3);
    inta_ = connect_pin(26);
    a0_   = pin(27);

    // Interrupt request inputs
    for (int i = 0; i < 8; ++i)
        ir_[i] = connect_pin(18 + i);

    // Interrupt output
    int_ = pin(17);

    // VCC
    Signal* vcc = socket.pin_signal(28);
    if (vcc) vcc->connect(this);

    // Pin directions for wiring visualization.
    declare_input(cs_); declare_input(wr_); declare_input(rd_);
    declare_input(inta_); declare_input(a0_);
    for (int i = 0; i < 8; ++i) declare_input(ir_[i]);
    declare_output(int_);

    // Data bus is bidirectional: Output during bus reads (~RD+~CS) and INTA,
    // Input during bus writes (~WR+~CS), HiZ otherwise.
    declare_bidir_block(
        {d_[0], d_[1], d_[2], d_[3], d_[4], d_[5], d_[6], d_[7]},
        BidirDir::HiZ | BidirDir::Input | BidirDir::Output,
        [this]() -> BidirDir {
            if (rd_.level() == Level::Low && cs_.level() == Level::Low)
                return BidirDir::Output;
            if (inta_.level() == Level::Low)
                return BidirDir::Output;
            if (wr_.level() == Level::Low && cs_.level() == Level::Low)
                return BidirDir::Input;
            return BidirDir::HiZ;
        });
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

void IC_8259A::on_cycle(Fiber /*caller*/) {
    Level wr_cur   = wr_.level();
    Level cs_cur   = cs_.level();
    Level rd_cur   = rd_.level();
    Level inta_cur = inta_.level();

    // Bus write: ~WR and ~CS both active, but deferred one eval.
    // ~WR falls at T2 but data propagates through xcvrs at T3.
    // Fire on the second eval where both are low (wr_prev_ already Low).
    if (wr_cur == Level::Low && cs_cur == Level::Low &&
        wr_prev_ == Level::Low && !write_latched_) {
        on_bus_write();
        write_latched_ = true;
    }

    if (wr_cur != Level::Low || cs_cur != Level::Low)
        write_latched_ = false;

    // Bus read: continuously drive data while ~RD and ~CS both active.
    // Edge-only driving fails when another driver (U8 nudge) overwrites AD
    // between the falling edge and the 8088's read_data() in a later wave.
    if (rd_cur == Level::Low && cs_cur == Level::Low)
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
        bool now_high = ir_[i].level() == Level::High;
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
    bool a0 = a0_.level() == Level::High;

    if (!a0 && (data & 0x10)) {
        // ICW1: A0=0, D4=1
        icw1_ = data;
        edge_triggered_ = !(data & 0x08);
        single_mode_    =  (data & 0x02);
        icw4_needed_    =  (data & 0x01);
        init_state_ = InitState::WaitICW2;
        initialized_ = false;

        imr_ = 0;
        isr_ = 0;
        irr_ = 0;
        auto_eoi_ = false;
        read_isr_ = false;
        inta_count_ = 0;
        inta_level_ = -1;

        int_.drive(Level::Low);
        return;
    }

    // Initialization sequence
    if (init_state_ == InitState::WaitICW2 && a0) {
        vector_base_ = data & 0xF8;
        if (!single_mode_)
            init_state_ = InitState::WaitICW3;
        else if (icw4_needed_)
            init_state_ = InitState::WaitICW4;
        else {
            init_state_ = InitState::Ready;
            initialized_ = true;
            evaluate_int();
        }
        return;
    }

    if (init_state_ == InitState::WaitICW3 && a0) {
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
        mode_8086_ = (data & 0x01);
        auto_eoi_  = (data & 0x02);
        init_state_ = InitState::Ready;
        initialized_ = true;

        for (int i = 0; i < 8; ++i) {
            if (ir_[i].level() == Level::High) {
                ir_prev_ |= (1 << i);
                if (edge_triggered_)
                    irr_ |= (1 << i);
            }
        }
        evaluate_int();
        return;
    }

    if (!initialized_) return;

    if (a0) {
        imr_ = data;
        evaluate_int();
        return;
    }

    if ((data & 0x18) == 0x00) {
        int eoi_type = (data >> 5) & 0x07;
        switch (eoi_type) {
            case 0x01: {
                int lvl = highest_priority_irq(isr_);
                if (lvl >= 0)
                    isr_ &= ~(1 << lvl);
                break;
            }
            case 0x03: {
                int lvl = data & 0x07;
                isr_ &= ~(1 << lvl);
                break;
            }
            case 0x05: {
                int lvl = highest_priority_irq(isr_);
                if (lvl >= 0)
                    isr_ &= ~(1 << lvl);
                break;
            }
            default:
                break;
        }
        evaluate_int();
    } else if ((data & 0x18) == 0x08) {
        if (data & 0x02)
            read_isr_ = (data & 0x01);
    }
}

void IC_8259A::on_bus_read() {
    if (!initialized_) return;
    bool a0 = a0_.level() == Level::High;

    uint8_t val = a0 ? imr_ : (read_isr_ ? isr_ : irr_);
    drive_data(val);
}

void IC_8259A::on_inta_falling() {
    if (!initialized_) return;

    inta_count_++;

    if (inta_count_ == 1) {
        inta_level_ = highest_priority_irq(irr_ & ~imr_);
        if (inta_level_ >= 0) {
            isr_ |= (1 << inta_level_);
            irr_ &= ~(1 << inta_level_);
            int_.drive(Level::Low);
        }
    } else if (inta_count_ == 2) {
        if (inta_level_ >= 0) {
            uint8_t vector;
            if (mode_8086_)
                vector = vector_base_ | inta_level_;
            else
                vector = vector_base_ | (inta_level_ << 2);
            drive_data(vector);

            if (auto_eoi_)
                isr_ &= ~(1 << inta_level_);
        }
        evaluate_int();
    }
}

void IC_8259A::evaluate_int() {
    if (!initialized_) return;

    uint8_t pending = irr_ & ~imr_;
    int req = highest_priority_irq(pending);

    if (req >= 0) {
        int svc = highest_priority_irq(isr_);
        if (svc < 0 || req < svc) {
            int_.drive(Level::High);
            return;
        }
    }

    int_.drive(Level::Low);
}

void IC_8259A::drive_data(uint8_t value) {
    for (int i = 0; i < 8; ++i)
        d_[i].drive((value >> i) & 1 ? Level::High : Level::Low);
}

void IC_8259A::release_data() {
    for (int i = 0; i < 8; ++i)
        d_[i].release();
}

uint8_t IC_8259A::read_data() const {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        if (d_[i].level() == Level::High)
            val |= (1 << i);
    }
    return val;
}

int IC_8259A::highest_priority_irq(uint8_t reg) const {
    for (int i = 0; i < 8; ++i) {
        if (reg & (1 << i))
            return i;
    }
    return -1;
}

} // namespace bench
