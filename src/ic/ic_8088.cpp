#include "ic/ic_8088.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace bench {

// 8086tiny constants
static constexpr int REG_AX = 0, REG_CX = 1, REG_DX = 2, REG_BX = 3;
static constexpr int REG_SP = 4, REG_BP = 5, REG_SI = 6, REG_DI = 7;
static constexpr int REG_ES = 8, REG_CS = 9, REG_SS = 10, REG_DS = 11;
static constexpr int REG_ZERO = 12, REG_SCRATCH = 13;

static constexpr int REG_AL = 0, REG_AH = 1, REG_CL = 2, REG_CH = 3;
static constexpr int REG_DL = 4, REG_DH = 5, REG_BL = 6, REG_BH = 7;

static constexpr int FLAG_CF = 40, FLAG_PF = 41, FLAG_AF = 42, FLAG_ZF = 43;
static constexpr int FLAG_SF = 44, FLAG_TF = 45, FLAG_IF = 46, FLAG_DF = 47;
static constexpr int FLAG_OF = 48;
// Note: FLAGS are stored as individual bytes in regs8[40..48].
// This means regs_ must be at least 49 bytes. We'll size it to 64 for safety.

// BIOS table indices
static constexpr int TABLE_XLAT_OPCODE = 8;
static constexpr int TABLE_XLAT_SUBFUNCTION = 9;
static constexpr int TABLE_STD_FLAGS = 10;
static constexpr int TABLE_PARITY_FLAG = 11;
static constexpr int TABLE_BASE_INST_SIZE = 12;
static constexpr int TABLE_I_W_SIZE = 13;
static constexpr int TABLE_I_MOD_SIZE = 14;
static constexpr int TABLE_COND_JUMP_DECODE_A = 15;
static constexpr int TABLE_COND_JUMP_DECODE_B = 16;
static constexpr int TABLE_COND_JUMP_DECODE_C = 17;
static constexpr int TABLE_COND_JUMP_DECODE_D = 18;
static constexpr int TABLE_FLAGS_BITFIELDS = 19;

static constexpr int FLAGS_UPDATE_SZP = 1;
static constexpr int FLAGS_UPDATE_AO_ARITH = 2;
static constexpr int FLAGS_UPDATE_OC_LOGIC = 4;

// Bus cycle status encoding (~S2, ~S1, ~S0) - accent: active low
static constexpr uint8_t BUS_INTA   = 0;  // 0,0,0
static constexpr uint8_t BUS_IOR    = 1;  // 0,0,1
static constexpr uint8_t BUS_IOW    = 2;  // 0,1,0
static constexpr uint8_t BUS_HALT   = 3;  // 0,1,1
static constexpr uint8_t BUS_FETCH  = 4;  // 1,0,0
static constexpr uint8_t BUS_MEMR   = 5;  // 1,0,1
static constexpr uint8_t BUS_MEMW   = 6;  // 1,1,0
static constexpr uint8_t BUS_PASSIVE = 7; // 1,1,1

IC_8088::IC_8088() : Component("8088") {}

void IC_8088::install(Socket& socket) {
    // AD0-AD7: pin16(AD0) .. pin9(AD7)
    for (int i = 0; i < 8; ++i)
        pin_ad_[i] = socket.pin_signal(16 - i);

    // A8-A14: pin8(A8) .. pin2(A14)
    for (int i = 0; i < 7; ++i)
        pin_a_upper_[i] = socket.pin_signal(8 - i);  // A8=pin8, A9=pin7, ..., A14=pin2

    // A15-A19: pin39(A15) .. pin35(A19)
    for (int i = 0; i < 5; ++i)
        pin_a_upper_[7 + i] = socket.pin_signal(39 - i);  // A15=pin39, ..., A19=pin35

    // Status outputs
    pin_s0_ = socket.pin_signal(26);   // ~S0
    pin_s1_ = socket.pin_signal(27);   // ~S1
    pin_s2_ = socket.pin_signal(28);   // ~S2

    // Queue status
    pin_qs0_ = socket.pin_signal(25);
    pin_qs1_ = socket.pin_signal(24);

    // Control inputs
    pin_clk_   = socket.pin_signal(19);
    pin_reset_ = socket.pin_signal(21);
    pin_ready_ = socket.pin_signal(22);
    pin_intr_  = socket.pin_signal(18);
    pin_nmi_   = socket.pin_signal(17);
    pin_test_  = socket.pin_signal(23);

    // VCC
    pin_vcc_ = socket.pin_signal(31);

    // Outputs
    pin_lock_  = socket.pin_signal(29);
    pin_rqgt0_ = socket.pin_signal(30);

    // Subscribe to inputs
    if (pin_clk_)   pin_clk_->connect(this);
    if (pin_reset_) pin_reset_->connect(this);
    if (pin_intr_)  pin_intr_->connect(this);
    if (pin_nmi_)   pin_nmi_->connect(this);
    if (pin_vcc_)   pin_vcc_->connect(this);

    spdlog::debug("[8088] installed into socket {}", socket.ref());
}

void IC_8088::on_signal_change(Signal& signal, Level old_level, Level new_level) {
    // CLK edge tracking -- wake the CPU execution loop
    if (&signal == pin_clk_) {
        clk_level_ = (new_level == Level::High);
        clk_sem_.release();
        return;
    }

    // NMI edge detection (rising edge)
    if (&signal == pin_nmi_) {
        if (new_level == Level::High && old_level != Level::High)
            nmi_pending_ = true;
        return;
    }
}

void IC_8088::run(std::stop_token stop) {
    // Wait for VCC
    while (!stop.stop_requested()) {
        wait_mailbox(stop);
        if (stop.stop_requested()) return;
        if (pin_vcc_ && pin_vcc_->level() == Level::High)
            break;
    }

    spdlog::info("[8088] VCC detected, waiting for RESET");

    // Wait for RESET to go High then Low (reset sequence)
    // The 8284A drives RESET high for a period after power-on.
    while (!stop.stop_requested()) {
        wait_mailbox(stop);
        if (stop.stop_requested()) return;
        if (pin_reset_ && pin_reset_->level() == Level::High)
            break;
    }

    // RESET is high -- initialize CPU
    cpu_reset();
    spdlog::info("[8088] RESET -- CS:IP = F000:0100");

    // Wait for RESET to drop
    while (!stop.stop_requested()) {
        wait_mailbox(stop);
        if (stop.stop_requested()) return;
        if (pin_reset_ && pin_reset_->level() != Level::High)
            break;
    }

    spdlog::info("[8088] RESET released, starting execution");

    // Drive status passive initially
    drive_status_passive();

    // Main instruction execution loop
    while (!stop.stop_requested()) {
        // Check VCC still up
        if (pin_vcc_ && pin_vcc_->level() != Level::High)
            break;

        execute();
    }

    // Release all outputs
    drive_status_passive();
    release_data();
    for (int i = 0; i < 12; ++i)
        if (pin_a_upper_[i]) pin_a_upper_[i]->release();
    if (pin_lock_) pin_lock_->release();
    if (pin_qs0_) pin_qs0_->release();
    if (pin_qs1_) pin_qs1_->release();

    spdlog::info("[8088] powered off");
}

// ========================================================================
// Bus operations -- every memory/IO access goes through the signal bus
// ========================================================================

void IC_8088::drive_address(uint32_t address) {
    // AD0-AD7 carry A0-A7 during T1
    for (int i = 0; i < 8; ++i) {
        if (pin_ad_[i])
            pin_ad_[i]->drive((address >> i) & 1 ? Level::High : Level::Low);
    }
    // A8-A19
    for (int i = 0; i < 12; ++i) {
        if (pin_a_upper_[i])
            pin_a_upper_[i]->drive((address >> (i + 8)) & 1 ? Level::High : Level::Low);
    }
}

void IC_8088::drive_data(uint8_t value) {
    for (int i = 0; i < 8; ++i) {
        if (pin_ad_[i])
            pin_ad_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
    }
}

uint8_t IC_8088::read_data() {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_ad_[i] && pin_ad_[i]->level() == Level::High)
            val |= (1 << i);
    }
    return val;
}

void IC_8088::release_data() {
    for (int i = 0; i < 8; ++i) {
        if (pin_ad_[i])
            pin_ad_[i]->release();
    }
}

void IC_8088::drive_status(uint8_t s2, uint8_t s1, uint8_t s0) {
    // Active low: status bits are inverted on the pins
    if (pin_s0_) pin_s0_->drive(s0 ? Level::High : Level::Low);
    if (pin_s1_) pin_s1_->drive(s1 ? Level::High : Level::Low);
    if (pin_s2_) pin_s2_->drive(s2 ? Level::High : Level::Low);
}

void IC_8088::drive_status_passive() {
    // Passive = ~S2=1, ~S1=1, ~S0=1 (all high = no bus cycle)
    if (pin_s0_) pin_s0_->drive(Level::High);
    if (pin_s1_) pin_s1_->drive(Level::High);
    if (pin_s2_) pin_s2_->drive(Level::High);
}

void IC_8088::wait_clk_rising() {
    // Wait until CLK goes high
    while (!clk_level_)
        clk_sem_.acquire();
    // Drain any extra releases
}

void IC_8088::wait_clk_falling() {
    // Wait until CLK goes low
    while (clk_level_)
        clk_sem_.acquire();
}

uint8_t IC_8088::bus_read_byte(uint32_t address) {
    // T1: Drive address + status
    drive_address(address & 0xFFFFF);
    drive_status((BUS_MEMR >> 2) & 1, (BUS_MEMR >> 1) & 1, BUS_MEMR & 1);
    wait_clk_rising();
    wait_clk_falling();

    // T2: Release AD bus (float for data input), ALE has latched address
    release_data();
    wait_clk_rising();
    wait_clk_falling();

    // T3: Wait for READY
    wait_clk_rising();
    // Insert Tw (wait states) while READY is low
    while (pin_ready_ && pin_ready_->level() != Level::High) {
        wait_clk_falling();
        wait_clk_rising();
    }
    wait_clk_falling();

    // T4: Read data from bus
    uint8_t data = read_data();
    drive_status_passive();
    wait_clk_rising();
    wait_clk_falling();

    return data;
}

void IC_8088::bus_write_byte(uint32_t address, uint8_t value) {
    // T1: Drive address + status
    drive_address(address & 0xFFFFF);
    drive_status((BUS_MEMW >> 2) & 1, (BUS_MEMW >> 1) & 1, BUS_MEMW & 1);
    wait_clk_rising();
    wait_clk_falling();

    // T2: Drive data onto AD bus
    drive_data(value);
    wait_clk_rising();
    wait_clk_falling();

    // T3: Wait for READY
    wait_clk_rising();
    while (pin_ready_ && pin_ready_->level() != Level::High) {
        wait_clk_falling();
        wait_clk_rising();
    }
    wait_clk_falling();

    // T4: Complete
    release_data();
    drive_status_passive();
    wait_clk_rising();
    wait_clk_falling();
}

uint16_t IC_8088::bus_read_word(uint32_t address) {
    // 8088 has 8-bit data bus: two byte reads
    uint8_t lo = bus_read_byte(address);
    uint8_t hi = bus_read_byte(address + 1);
    return lo | (hi << 8);
}

void IC_8088::bus_write_word(uint32_t address, uint16_t value) {
    bus_write_byte(address, value & 0xFF);
    bus_write_byte(address + 1, (value >> 8) & 0xFF);
}

uint8_t IC_8088::io_read_byte(uint16_t port) {
    // T1: Drive 16-bit port address (upper 4 bits = 0)
    drive_address(port);
    drive_status((BUS_IOR >> 2) & 1, (BUS_IOR >> 1) & 1, BUS_IOR & 1);
    wait_clk_rising();
    wait_clk_falling();

    // T2: Release AD bus
    release_data();
    wait_clk_rising();
    wait_clk_falling();

    // T3: Wait for READY
    wait_clk_rising();
    while (pin_ready_ && pin_ready_->level() != Level::High) {
        wait_clk_falling();
        wait_clk_rising();
    }
    wait_clk_falling();

    // T4: Read data
    uint8_t data = read_data();
    drive_status_passive();
    wait_clk_rising();
    wait_clk_falling();

    return data;
}

void IC_8088::io_write_byte(uint16_t port, uint8_t value) {
    drive_address(port);
    drive_status((BUS_IOW >> 2) & 1, (BUS_IOW >> 1) & 1, BUS_IOW & 1);
    wait_clk_rising();
    wait_clk_falling();

    drive_data(value);
    wait_clk_rising();
    wait_clk_falling();

    wait_clk_rising();
    while (pin_ready_ && pin_ready_->level() != Level::High) {
        wait_clk_falling();
        wait_clk_rising();
    }
    wait_clk_falling();

    release_data();
    drive_status_passive();
    wait_clk_rising();
    wait_clk_falling();
}

// ========================================================================
// CPU core -- ported from 8086tiny
// ========================================================================

void IC_8088::cpu_reset() {
    std::memset(regs_, 0, sizeof(regs_));
    regs16()[REG_CS] = 0xF000;
    reg_ip_ = 0x0100;
    regs8()[FLAG_TF] = 0;

    seg_override_en_ = 0;
    rep_override_en_ = 0;
    trap_flag_ = 0;
    nmi_pending_ = false;
    nmi_prev_ = false;
}

int IC_8088::set_CF(int new_CF) {
    return regs8()[FLAG_CF] = !!new_CF;
}

int IC_8088::set_AF(int new_AF) {
    return regs8()[FLAG_AF] = !!new_AF;
}

int IC_8088::set_OF(int new_OF) {
    return regs8()[FLAG_OF] = !!new_OF;
}

void IC_8088::set_AF_OF_arith() {
    set_AF((op_source_ ^= op_dest_ ^ op_result_) & 0x10);
    if (op_result_ == (int)op_dest_)
        set_OF(0);
    else
        set_OF(1 & (regs8()[FLAG_CF] ^ op_source_ >> (8 * (i_w_ + 1) - 1)));
}

void IC_8088::make_flags() {
    scratch_uint_ = 0xF002;
    for (int i = 8; i >= 0; --i)
        scratch_uint_ += regs8()[FLAG_CF + i] << bios_table_[TABLE_FLAGS_BITFIELDS][i];
}

void IC_8088::set_flags(int new_flags) {
    for (int i = 8; i >= 0; --i)
        regs8()[FLAG_CF + i] = !!(1 << bios_table_[TABLE_FLAGS_BITFIELDS][i] & new_flags);
}

void IC_8088::set_opcode(uint8_t opcode) {
    xlat_opcode_id_ = bios_table_[TABLE_XLAT_OPCODE][raw_opcode_id_ = opcode];
    extra_ = bios_table_[TABLE_XLAT_SUBFUNCTION][opcode];
    i_mod_size_ = bios_table_[TABLE_I_MOD_SIZE][opcode];
    set_flags_type_ = bios_table_[TABLE_STD_FLAGS][opcode];
}

void IC_8088::pc_interrupt(uint8_t interrupt_num) {
    set_opcode(0xCD);

    make_flags();
    // PUSH flags
    regs16()[REG_SP] -= 2;
    bus_write_word(16 * regs16()[REG_SS] + regs16()[REG_SP], scratch_uint_);
    // PUSH CS
    regs16()[REG_SP] -= 2;
    bus_write_word(16 * regs16()[REG_SS] + regs16()[REG_SP], regs16()[REG_CS]);
    // PUSH IP
    regs16()[REG_SP] -= 2;
    bus_write_word(16 * regs16()[REG_SS] + regs16()[REG_SP], reg_ip_);

    // Load CS:IP from IVT
    regs16()[REG_CS] = bus_read_word(4 * interrupt_num + 2);
    reg_ip_ = bus_read_word(4 * interrupt_num);

    regs8()[FLAG_TF] = 0;
    regs8()[FLAG_IF] = 0;
}

int IC_8088::AAA_AAS(int which_operation) {
    int adj = ((regs8()[REG_AL] & 0x0F) > 9) || regs8()[FLAG_AF];
    set_AF(adj);
    set_CF(adj);
    if (adj)
        regs16()[REG_AX] += 262 * which_operation;
    regs8()[REG_AL] &= 0x0F;
    return regs8()[REG_AL];
}

void IC_8088::execute() {
    // Fetch opcode byte via bus read
    uint32_t cs_ip = 16u * regs16()[REG_CS] + reg_ip_;
    if (cs_ip == 0) {
        // CS:IP = 0:0 means halt (8086tiny convention)
        spdlog::info("[8088] CS:IP = 0000:0000 -- halted");
        // TODO: drive HALT status on bus
        return;
    }

    uint8_t opcode = bus_read_byte(cs_ip);
    set_opcode(opcode);

    // Extract i_w and i_d fields
    i_w_ = (i_reg4bit_ = raw_opcode_id_ & 7) & 1;
    i_d_ = i_reg4bit_ / 2 & 1;

    // Fetch additional instruction bytes (up to 5 more for longest instructions)
    uint8_t inst_bytes[6];
    inst_bytes[0] = opcode;
    // Prefetch bytes 1-5
    for (int i = 1; i < 6; ++i)
        inst_bytes[i] = bus_read_byte(cs_ip + i);

    i_data0_ = (int16_t)(inst_bytes[1] | (inst_bytes[2] << 8));
    i_data1_ = (int16_t)(inst_bytes[2] | (inst_bytes[3] << 8));
    i_data2_ = (int16_t)(inst_bytes[3] | (inst_bytes[4] << 8));

    // Segment/REP override countdown
    if (seg_override_en_) seg_override_en_--;
    if (rep_override_en_) rep_override_en_--;

    // Decode mod/rm/reg if needed
    if (i_mod_size_) {
        i_mod_ = (i_data0_ & 0xFF) >> 6;
        i_rm_ = i_data0_ & 7;
        i_reg_ = (i_data0_ >> 3) & 7;

        if ((!i_mod_ && i_rm_ == 6) || (i_mod_ == 2))
            i_data2_ = (int16_t)(inst_bytes[4] | (inst_bytes[5] << 8));
        else if (i_mod_ != 1)
            i_data2_ = i_data1_;
        else
            i_data1_ = (int8_t)inst_bytes[2];

        // DECODE_RM_REG -- compute effective addresses
        // This is the heart of address calculation, adapted to use bus reads
        // TODO: full DECODE_RM_REG port
    }

    // TODO: Port the full switch(xlat_opcode_id_) instruction execution from 8086tiny.
    // Each case that accesses mem[] must use bus_read_byte/bus_write_byte instead.
    // Each case that accesses io_ports[] must use io_read_byte/io_write_byte instead.

    // Advance IP
    reg_ip_ += (i_mod_ * (i_mod_ != 3) + 2 * (!i_mod_ && i_rm_ == 6)) * i_mod_size_
             + bios_table_[TABLE_BASE_INST_SIZE][raw_opcode_id_]
             + bios_table_[TABLE_I_W_SIZE][raw_opcode_id_] * (i_w_ + 1);

    // Update flags
    if (set_flags_type_ & FLAGS_UPDATE_SZP) {
        regs8()[FLAG_SF] = 1 & ((i_w_ ? (int16_t)op_result_ : (int8_t)op_result_) >> (8 * (i_w_ + 1) - 1));
        regs8()[FLAG_ZF] = !op_result_;
        regs8()[FLAG_PF] = bios_table_[TABLE_PARITY_FLAG][(uint8_t)op_result_];

        if (set_flags_type_ & FLAGS_UPDATE_AO_ARITH)
            set_AF_OF_arith();
        if (set_flags_type_ & FLAGS_UPDATE_OC_LOGIC)
            set_CF(0), set_OF(0);
    }

    // Trap flag
    if (trap_flag_)
        pc_interrupt(1);
    trap_flag_ = regs8()[FLAG_TF];

    // Check for pending interrupts
    if (regs8()[FLAG_IF] && !seg_override_en_ && !rep_override_en_ && !regs8()[FLAG_TF]) {
        // NMI (edge-triggered, higher priority)
        if (nmi_pending_) {
            nmi_pending_ = false;
            pc_interrupt(2);
        }
        // INTR (level-triggered)
        else if (pin_intr_ && pin_intr_->level() == Level::High) {
            // TODO: INTA bus cycle to read vector from 8259A
            // For now, placeholder
        }
    }
}

} // namespace bench
