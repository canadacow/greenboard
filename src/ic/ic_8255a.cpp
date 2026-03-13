#include "ic/ic_8255a.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_8255A::IC_8255A() : CallbackComponent("8255A") {}

void IC_8255A::install(Socket& socket) {
    // Data bus: D0=pin34, D1=pin33, ..., D7=pin27
    for (int i = 0; i < 8; ++i)
        pin_d_[i] = socket.pin_signal(34 - i);

    // Port A: PA0=pin4, PA1=pin3, PA2=pin2, PA3=pin1, PA4=pin40, PA5=pin39, PA6=pin38, PA7=pin37
    pin_pa_[0] = socket.pin_signal(4);
    pin_pa_[1] = socket.pin_signal(3);
    pin_pa_[2] = socket.pin_signal(2);
    pin_pa_[3] = socket.pin_signal(1);
    pin_pa_[4] = socket.pin_signal(40);
    pin_pa_[5] = socket.pin_signal(39);
    pin_pa_[6] = socket.pin_signal(38);
    pin_pa_[7] = socket.pin_signal(37);

    // Port B: PB0=pin18 .. PB7=pin25
    for (int i = 0; i < 8; ++i)
        pin_pb_[i] = socket.pin_signal(18 + i);

    // Port C: PC0=pin14, PC1=pin15, PC2=pin16, PC3=pin17
    //         PC4=pin13, PC5=pin12, PC6=pin11, PC7=pin10
    pin_pc_[0] = socket.pin_signal(14);
    pin_pc_[1] = socket.pin_signal(15);
    pin_pc_[2] = socket.pin_signal(16);
    pin_pc_[3] = socket.pin_signal(17);
    pin_pc_[4] = socket.pin_signal(13);
    pin_pc_[5] = socket.pin_signal(12);
    pin_pc_[6] = socket.pin_signal(11);
    pin_pc_[7] = socket.pin_signal(10);

    // Control pins
    pin_cs_    = socket.pin_signal(6);   // ~CS
    pin_rd_    = socket.pin_signal(5);   // ~RD
    pin_wr_    = socket.pin_signal(36);  // ~WR
    pin_a0_    = socket.pin_signal(9);   // A0
    pin_a1_    = socket.pin_signal(8);   // A1
    pin_reset_ = socket.pin_signal(35);  // RESET
    pin_vcc_   = socket.pin_signal(26);  // VCC

    // Subscribe to control signals
    if (pin_cs_)    pin_cs_->connect(this);
    if (pin_wr_)    pin_wr_->connect(this);
    if (pin_rd_)    pin_rd_->connect(this);
    if (pin_reset_) pin_reset_->connect(this);
    if (pin_vcc_)   pin_vcc_->connect(this);
}

void IC_8255A::on_signal_change(bool rising, bool /*falling*/) {
    if (!rising) return;  // compute once per cycle
    Level reset_cur = pin_reset_ ? pin_reset_->level() : Level::HiZ;
    Level wr_cur = pin_wr_ ? pin_wr_->level() : Level::HiZ;
    Level cs_cur = pin_cs_ ? pin_cs_->level() : Level::HiZ;
    Level rd_cur = pin_rd_ ? pin_rd_->level() : Level::HiZ;

    // RESET rising edge
    if (reset_cur == Level::High && reset_prev_ != Level::High)
        on_reset();

    // Bus write: ~WR falling while ~CS active
    if (wr_cur == Level::Low && wr_prev_ != Level::Low && cs_cur == Level::Low)
        on_bus_write();
    if (cs_cur == Level::Low && cs_prev_ != Level::Low && wr_cur == Level::Low)
        on_bus_write();

    // Bus read: ~RD falling while ~CS active
    if (rd_cur == Level::Low && rd_prev_ != Level::Low && cs_cur == Level::Low)
        on_bus_read();
    if (cs_cur == Level::Low && cs_prev_ != Level::Low && rd_cur == Level::Low)
        on_bus_read();

    // Release data bus when ~RD or ~CS goes inactive
    if ((rd_cur == Level::High && rd_prev_ != Level::High) ||
        (cs_cur == Level::High && cs_prev_ != Level::High))
        release_data();

    reset_prev_ = reset_cur;
    wr_prev_ = wr_cur;
    cs_prev_ = cs_cur;
    rd_prev_ = rd_cur;
}

void IC_8255A::on_reset() {
    // Reset: all ports set to input mode, all latches cleared
    control_ = 0x9B;
    latch_a_ = 0;
    latch_b_ = 0;
    latch_c_ = 0;
    pa_input_ = true;
    pb_input_ = true;
    pc_upper_input_ = true;
    pc_lower_input_ = true;

    // Release all port pins (go HiZ since all are now inputs)
    for (int i = 0; i < 8; ++i) {
        if (pin_pa_[i]) pin_pa_[i]->release();
        if (pin_pb_[i]) pin_pb_[i]->release();
        if (pin_pc_[i]) pin_pc_[i]->release();
    }

    spdlog::debug("[8255A] reset -- all ports input");
}

void IC_8255A::on_bus_write() {
    uint8_t data = read_data();
    bool a0 = pin_a0_ && pin_a0_->level() == Level::High;
    bool a1 = pin_a1_ && pin_a1_->level() == Level::High;
    int port = (a1 ? 2 : 0) | (a0 ? 1 : 0);

    switch (port) {
        case 0:  // Port A
            latch_a_ = data;
            if (!pa_input_) write_port_a(data);
            break;

        case 1:  // Port B
            latch_b_ = data;
            if (!pb_input_) write_port_b(data);
            break;

        case 2:  // Port C
            latch_c_ = data;
            write_port_c(data);
            break;

        case 3:  // Control register
            if (data & 0x80) {
                // Mode set
                control_ = data;
                pa_input_       = (data & 0x10) != 0;
                pc_upper_input_ = (data & 0x08) != 0;
                pb_input_       = (data & 0x02) != 0;
                pc_lower_input_ = (data & 0x01) != 0;

                // Clear output latches
                latch_a_ = 0;
                latch_b_ = 0;
                latch_c_ = 0;

                // Drive output ports, release input ports
                if (!pa_input_) write_port_a(0); else {
                    for (int i = 0; i < 8; ++i)
                        if (pin_pa_[i]) pin_pa_[i]->release();
                }
                if (!pb_input_) write_port_b(0); else {
                    for (int i = 0; i < 8; ++i)
                        if (pin_pb_[i]) pin_pb_[i]->release();
                }
                write_port_c(0);  // handles mixed input/output

                spdlog::debug("[8255A] control={:#04x} PA={} PB={} PCu={} PCl={}",
                              data,
                              pa_input_ ? "in" : "out",
                              pb_input_ ? "in" : "out",
                              pc_upper_input_ ? "in" : "out",
                              pc_lower_input_ ? "in" : "out");
            } else {
                // Bit set/reset on Port C
                int bit = (data >> 1) & 0x07;
                if (data & 0x01)
                    latch_c_ |= (1 << bit);
                else
                    latch_c_ &= ~(1 << bit);
                write_port_c(latch_c_);
            }
            break;
    }
}

void IC_8255A::on_bus_read() {
    bool a0 = pin_a0_ && pin_a0_->level() == Level::High;
    bool a1 = pin_a1_ && pin_a1_->level() == Level::High;
    int port = (a1 ? 2 : 0) | (a0 ? 1 : 0);

    switch (port) {
        case 0:  drive_data(pa_input_ ? read_port_a() : latch_a_); break;
        case 1:  drive_data(pb_input_ ? read_port_b() : latch_b_); break;
        case 2:  drive_data(read_port_c()); break;
        case 3:  drive_data(control_); break;  // some 8255 variants allow reading control
    }
}

uint8_t IC_8255A::read_port_a() const {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_pa_[i] && pin_pa_[i]->level() == Level::High)
            val |= (1 << i);
    }
    return val;
}

uint8_t IC_8255A::read_port_b() const {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_pb_[i] && pin_pb_[i]->level() == Level::High)
            val |= (1 << i);
    }
    return val;
}

uint8_t IC_8255A::read_port_c() const {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        bool is_input = (i >= 4) ? pc_upper_input_ : pc_lower_input_;
        if (is_input) {
            // Input: read pin level
            if (pin_pc_[i] && pin_pc_[i]->level() == Level::High)
                val |= (1 << i);
        } else {
            // Output: return latch value
            val |= (latch_c_ & (1 << i));
        }
    }
    return val;
}

void IC_8255A::write_port_a(uint8_t value) {
    for (int i = 0; i < 8; ++i) {
        if (pin_pa_[i])
            pin_pa_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
    }
}

void IC_8255A::write_port_b(uint8_t value) {
    for (int i = 0; i < 8; ++i) {
        if (pin_pb_[i])
            pin_pb_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
    }
}

void IC_8255A::write_port_c(uint8_t value) {
    // Port C has split direction: upper and lower nibbles independent
    for (int i = 0; i < 4; ++i) {
        if (!pc_lower_input_ && pin_pc_[i])
            pin_pc_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
        else if (pc_lower_input_ && pin_pc_[i])
            pin_pc_[i]->release();
    }
    for (int i = 4; i < 8; ++i) {
        if (!pc_upper_input_ && pin_pc_[i])
            pin_pc_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
        else if (pc_upper_input_ && pin_pc_[i])
            pin_pc_[i]->release();
    }
}

void IC_8255A::drive_data(uint8_t value) {
    for (int i = 0; i < 8; ++i) {
        if (pin_d_[i])
            pin_d_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
    }
}

void IC_8255A::release_data() {
    for (int i = 0; i < 8; ++i) {
        if (pin_d_[i])
            pin_d_[i]->release();
    }
}

uint8_t IC_8255A::read_data() const {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_d_[i] && pin_d_[i]->level() == Level::High)
            val |= (1 << i);
    }
    return val;
}

} // namespace bench
