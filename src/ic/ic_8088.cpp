#include "ic/ic_8088.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace bench {

// Register indices (16-bit)
static constexpr int REG_AX = 0, REG_CX = 1, REG_DX = 2, REG_BX = 3;
static constexpr int REG_SP = 4, REG_BP = 5, REG_SI = 6, REG_DI = 7;
static constexpr int REG_ES = 8, REG_CS = 9, REG_SS = 10, REG_DS = 11;
static constexpr int REG_ZERO = 12, REG_SCRATCH = 13;

// Register indices (8-bit)
static constexpr int REG_AL = 0, REG_AH = 1, REG_CL = 2, REG_CH = 3;
static constexpr int REG_DL = 4, REG_DH = 5, REG_BL = 6, REG_BH = 7;

// Flags (stored as individual bytes in regs8[40..48])
static constexpr int FLAG_CF = 40, FLAG_PF = 41, FLAG_AF = 42, FLAG_ZF = 43;
static constexpr int FLAG_SF = 44, FLAG_TF = 45, FLAG_IF = 46, FLAG_DF = 47;
static constexpr int FLAG_OF = 48;

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

// Bus cycle status encoding (~S2, ~S1, ~S0)
static constexpr uint8_t BUS_INTA    = 0;  // 0,0,0
static constexpr uint8_t BUS_IOR     = 1;  // 0,0,1
static constexpr uint8_t BUS_IOW     = 2;  // 0,1,0
static constexpr uint8_t BUS_HALT    = 3;  // 0,1,1
static constexpr uint8_t BUS_FETCH   = 4;  // 1,0,0
static constexpr uint8_t BUS_MEMR    = 5;  // 1,0,1
static constexpr uint8_t BUS_MEMW    = 6;  // 1,1,0
static constexpr uint8_t BUS_PASSIVE = 7;  // 1,1,1

IC_8088::IC_8088(uint16_t start_cs, uint16_t start_ip)
    : Component("8088"), start_cs_(start_cs), start_ip_(start_ip) {}

void IC_8088::install(Socket& socket) {
    for (int i = 0; i < 8; ++i)
        pin_ad_[i] = socket.pin_signal(16 - i);
    for (int i = 0; i < 7; ++i)
        pin_a_upper_[i] = socket.pin_signal(8 - i);
    for (int i = 0; i < 5; ++i)
        pin_a_upper_[7 + i] = socket.pin_signal(39 - i);

    pin_s0_ = socket.pin_signal(26);
    pin_s1_ = socket.pin_signal(27);
    pin_s2_ = socket.pin_signal(28);
    pin_qs0_ = socket.pin_signal(25);
    pin_qs1_ = socket.pin_signal(24);
    pin_clk_   = socket.pin_signal(19);
    pin_reset_ = socket.pin_signal(21);
    pin_ready_ = socket.pin_signal(22);
    pin_intr_  = socket.pin_signal(18);
    pin_nmi_   = socket.pin_signal(17);
    pin_test_  = socket.pin_signal(23);
    pin_vcc_   = socket.pin_signal(31);
    pin_lock_  = socket.pin_signal(29);
    pin_rqgt0_ = socket.pin_signal(30);

    if (pin_clk_)   pin_clk_->connect(this);
    if (pin_reset_) pin_reset_->connect(this);
    if (pin_nmi_)   pin_nmi_->connect(this);
    if (pin_vcc_)   pin_vcc_->connect(this);

    spdlog::debug("[8088] installed into socket {}", socket.ref());
}

void IC_8088::on_signal_change() {
    // CLK edge detection
    if (pin_clk_) {
        Level cur = pin_clk_->level();
        if (cur == Level::High && clk_prev_ != Level::High)
            clk_rose_ = true;
        if (cur == Level::Low && clk_prev_ != Level::Low)
            clk_fell_ = true;
        clk_prev_ = cur;
    }
    // NMI rising edge detection
    if (pin_nmi_) {
        Level cur = pin_nmi_->level();
        if (cur == Level::High && nmi_prev_ != Level::High)
            nmi_pending_ = true;
        nmi_prev_ = cur;
    }
}

void IC_8088::run(std::stop_token stop) {
    stop_ = stop;

    while (!stop.stop_requested()) {
        wait_mailbox(stop);
        if (stop.stop_requested()) return;
        if (pin_vcc_ && pin_vcc_->level() == Level::High) break;
    }
    spdlog::info("[8088] VCC detected, waiting for RESET");

    // Sample RESET on CLK edges, like the real chip.
    // Stay in reset while RESET is High. Start executing when it goes Low.
    // If we missed the pulse entirely (RESET already Low), proceed immediately.
    cpu_reset();

    if (pin_reset_ && pin_reset_->level() == Level::High) {
        // RESET is currently asserted -- wait for it to deassert.
        spdlog::info("[8088] RESET asserted -- CS:IP = {:04X}:{:04X}", start_cs_, start_ip_);
        while (!stop.stop_requested()) {
            wait_mailbox(stop);
            if (stop.stop_requested()) return;
            if (pin_reset_->level() != Level::High) break;
        }
    } else {
        // RESET pulse already completed (or never happened).
        spdlog::info("[8088] RESET complete -- CS:IP = {:04X}:{:04X}", start_cs_, start_ip_);
    }

    spdlog::info("[8088] starting execution");

    drive_status_passive();

    while (!stop.stop_requested()) {
        if (pin_vcc_ && pin_vcc_->level() != Level::High) break;
        on_signal_change();  // process NMI, CLK edges
        execute();
    }

    drive_status_passive();
    release_data();
    for (int i = 0; i < 12; ++i)
        if (pin_a_upper_[i]) pin_a_upper_[i]->release();
    if (pin_lock_) pin_lock_->release();
    if (pin_qs0_) pin_qs0_->release();
    if (pin_qs1_) pin_qs1_->release();
}

// ========================================================================
// Bus operations
// ========================================================================

void IC_8088::drive_address(uint32_t address) {
    for (int i = 0; i < 8; ++i)
        if (pin_ad_[i])
            pin_ad_[i]->drive((address >> i) & 1 ? Level::High : Level::Low);
    for (int i = 0; i < 12; ++i)
        if (pin_a_upper_[i])
            pin_a_upper_[i]->drive((address >> (i + 8)) & 1 ? Level::High : Level::Low);
    spdlog::debug("[8088] drive_address 0x{:05X} ad[0]@{} level={}", address,
        (void*)pin_ad_[0], pin_ad_[0] ? (int)pin_ad_[0]->level() : -1);
}

void IC_8088::drive_data(uint8_t value) {
    for (int i = 0; i < 8; ++i)
        if (pin_ad_[i])
            pin_ad_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
}

uint8_t IC_8088::read_data() {
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i)
        if (pin_ad_[i] && pin_ad_[i]->level() == Level::High)
            val |= (1 << i);
    spdlog::debug("[8088] read_data -> 0x{:02X} (AD levels: {}{}{}{}{}{}{}{})",
        val,
        pin_ad_[7] ? (int)pin_ad_[7]->level() : -1,
        pin_ad_[6] ? (int)pin_ad_[6]->level() : -1,
        pin_ad_[5] ? (int)pin_ad_[5]->level() : -1,
        pin_ad_[4] ? (int)pin_ad_[4]->level() : -1,
        pin_ad_[3] ? (int)pin_ad_[3]->level() : -1,
        pin_ad_[2] ? (int)pin_ad_[2]->level() : -1,
        pin_ad_[1] ? (int)pin_ad_[1]->level() : -1,
        pin_ad_[0] ? (int)pin_ad_[0]->level() : -1);
    return val;
}

void IC_8088::release_data() {
    for (int i = 0; i < 8; ++i)
        if (pin_ad_[i]) pin_ad_[i]->release();
}

void IC_8088::drive_status(uint8_t s2, uint8_t s1, uint8_t s0) {
    if (pin_s0_) pin_s0_->drive(s0 ? Level::High : Level::Low);
    if (pin_s1_) pin_s1_->drive(s1 ? Level::High : Level::Low);
    if (pin_s2_) pin_s2_->drive(s2 ? Level::High : Level::Low);
}

void IC_8088::drive_status_passive() {
    if (pin_s0_) pin_s0_->drive(Level::High);
    if (pin_s1_) pin_s1_->drive(Level::High);
    if (pin_s2_) pin_s2_->drive(Level::High);
}

void IC_8088::wait_clk_rising() {
    while (!clk_rose_ && !stop_.stop_requested()) {
        wait_mailbox(stop_);
        on_signal_change();
    }
    clk_rose_ = false;
}

void IC_8088::wait_clk_falling() {
    while (!clk_fell_ && !stop_.stop_requested()) {
        wait_mailbox(stop_);
        on_signal_change();
    }
    clk_fell_ = false;
}

uint8_t IC_8088::bus_read_byte(uint32_t address) {
    if (stop_.stop_requested()) return 0;
    spdlog::debug("[8088] bus_read_byte(0x{:05X}) -- drive status MEMR", address & 0xFFFFF);
    drive_status((BUS_MEMR >> 2) & 1, (BUS_MEMR >> 1) & 1, BUS_MEMR & 1);
    clk_rose_ = false; clk_fell_ = false;  // discard stale edges
    spdlog::debug("[8088]   wait T1 rise...");
    wait_clk_rising();
    spdlog::debug("[8088]   T1 rise -- drive address 0x{:05X}", address & 0xFFFFF);
    drive_address(address & 0xFFFFF);
    spdlog::debug("[8088]   wait T1 fall (ALE)...");
    wait_clk_falling();
    spdlog::debug("[8088]   T1 fall -- release AD");
    release_data();
    spdlog::debug("[8088]   wait T2 rise...");
    wait_clk_rising();
    spdlog::debug("[8088]   T2 rise");
    wait_clk_falling();
    spdlog::debug("[8088]   T2 fall");
    spdlog::debug("[8088]   wait T3 rise...");
    wait_clk_rising();
    spdlog::debug("[8088]   T3 rise -- drive status passive");
    drive_status_passive();
    while (pin_ready_ && pin_ready_->level() != Level::High
           && !stop_.stop_requested()) {
        spdlog::debug("[8088]   Tw (READY not high)");
        wait_clk_falling(); wait_clk_rising();
    }
    spdlog::debug("[8088]   wait T3 fall (sample data)...");
    wait_clk_falling();
    uint8_t data = read_data();
    spdlog::debug("[8088]   T3 fall -- READ 0x{:05X} -> 0x{:02X}", address & 0xFFFFF, data);
    spdlog::debug("[8088]   wait T4...");
    wait_clk_rising(); wait_clk_falling();
    spdlog::debug("[8088]   T4 done");
    return data;
}

void IC_8088::bus_write_byte(uint32_t address, uint8_t value) {
    if (stop_.stop_requested()) return;
    spdlog::debug("[8088] bus_write_byte(0x{:05X}, 0x{:02X}) -- drive status MEMW", address & 0xFFFFF, value);
    drive_status((BUS_MEMW >> 2) & 1, (BUS_MEMW >> 1) & 1, BUS_MEMW & 1);
    clk_rose_ = false; clk_fell_ = false;  // discard stale edges
    spdlog::debug("[8088]   wait T1 rise...");
    wait_clk_rising();
    spdlog::debug("[8088]   T1 rise -- drive address 0x{:05X}", address & 0xFFFFF);
    drive_address(address & 0xFFFFF);
    spdlog::debug("[8088]   wait T1 fall (ALE)...");
    wait_clk_falling();
    spdlog::debug("[8088]   T1 fall -- drive write data 0x{:02X}", value);
    drive_data(value);
    spdlog::debug("[8088]   wait T2 rise...");
    wait_clk_rising();
    spdlog::debug("[8088]   T2 rise");
    wait_clk_falling();
    spdlog::debug("[8088]   T2 fall");
    spdlog::debug("[8088]   wait T3 rise...");
    wait_clk_rising();
    spdlog::debug("[8088]   T3 rise -- drive status passive");
    drive_status_passive();
    while (pin_ready_ && pin_ready_->level() != Level::High
           && !stop_.stop_requested()) {
        spdlog::debug("[8088]   Tw (READY not high)");
        wait_clk_falling(); wait_clk_rising();
    }
    spdlog::debug("[8088]   wait T3 fall...");
    wait_clk_falling();
    spdlog::debug("[8088]   T3 fall -- WRITE 0x{:05X} <- 0x{:02X}", address & 0xFFFFF, value);
    release_data();
    spdlog::debug("[8088]   wait T4...");
    wait_clk_rising(); wait_clk_falling();
    spdlog::debug("[8088]   T4 done");
}

uint16_t IC_8088::bus_read_word(uint32_t address) {
    uint8_t lo = bus_read_byte(address);
    uint8_t hi = bus_read_byte(address + 1);
    return lo | (hi << 8);
}

void IC_8088::bus_write_word(uint32_t address, uint16_t value) {
    bus_write_byte(address, value & 0xFF);
    bus_write_byte(address + 1, (value >> 8) & 0xFF);
}

uint8_t IC_8088::io_read_byte(uint16_t port) {
    if (stop_.stop_requested()) return 0;
    drive_status((BUS_IOR >> 2) & 1, (BUS_IOR >> 1) & 1, BUS_IOR & 1);
    clk_rose_ = false; clk_fell_ = false;
    wait_clk_rising();
    drive_address(port);
    wait_clk_falling();
    release_data();
    wait_clk_rising(); wait_clk_falling();
    wait_clk_rising();
    drive_status_passive();
    while (pin_ready_ && pin_ready_->level() != Level::High
           && !stop_.stop_requested()) {
        wait_clk_falling(); wait_clk_rising();
    }
    wait_clk_falling();
    uint8_t data = read_data();
    wait_clk_rising(); wait_clk_falling();
    return data;
}

void IC_8088::io_write_byte(uint16_t port, uint8_t value) {
    if (stop_.stop_requested()) return;
    drive_status((BUS_IOW >> 2) & 1, (BUS_IOW >> 1) & 1, BUS_IOW & 1);
    clk_rose_ = false; clk_fell_ = false;
    wait_clk_rising();
    drive_address(port);
    wait_clk_falling();
    drive_data(value);
    wait_clk_rising(); wait_clk_falling();
    wait_clk_rising();
    drive_status_passive();
    while (pin_ready_ && pin_ready_->level() != Level::High
           && !stop_.stop_requested()) {
        wait_clk_falling(); wait_clk_rising();
    }
    wait_clk_falling();
    release_data();
    wait_clk_rising(); wait_clk_falling();
}

// ========================================================================
// Memory routing: register file or bus
// ========================================================================

uint8_t IC_8088::rmem8(uint32_t addr) {
    if (addr >= REGS_BASE && addr < REGS_BASE + sizeof(regs_))
        return regs_[addr - REGS_BASE];
    return bus_read_byte(addr);
}

uint16_t IC_8088::rmem16(uint32_t addr) {
    if (addr >= REGS_BASE && addr < REGS_BASE + sizeof(regs_) - 1) {
        uint32_t off = addr - REGS_BASE;
        return regs_[off] | (regs_[off + 1] << 8);
    }
    return bus_read_word(addr);
}

void IC_8088::wmem8(uint32_t addr, uint8_t val) {
    if (addr >= REGS_BASE && addr < REGS_BASE + sizeof(regs_)) {
        regs_[addr - REGS_BASE] = val;
        return;
    }
    bus_write_byte(addr, val);
}

void IC_8088::wmem16(uint32_t addr, uint16_t val) {
    if (addr >= REGS_BASE && addr < REGS_BASE + sizeof(regs_) - 1) {
        uint32_t off = addr - REGS_BASE;
        regs_[off] = val & 0xFF;
        regs_[off + 1] = (val >> 8) & 0xFF;
        return;
    }
    bus_write_word(addr, val);
}

uint32_t IC_8088::rmem(uint32_t addr) {
    return i_w_ ? rmem16(addr) : rmem8(addr);
}

void IC_8088::wmem(uint32_t addr, uint32_t val) {
    if (i_w_) wmem16(addr, (uint16_t)val);
    else      wmem8(addr, (uint8_t)val);
}

// ========================================================================
// Stack
// ========================================================================

void IC_8088::push16(uint16_t val) {
    regs16()[REG_SP] -= 2;
    wmem16(16u * regs16()[REG_SS] + regs16()[REG_SP], val);
}

uint16_t IC_8088::pop16() {
    uint16_t val = rmem16(16u * regs16()[REG_SS] + regs16()[REG_SP]);
    regs16()[REG_SP] += 2;
    return val;
}

// ========================================================================
// Decode helpers
// ========================================================================

uint32_t IC_8088::get_reg_addr(int reg_id) {
    return REGS_BASE + (i_w_ ? 2 * reg_id : (2 * reg_id + (reg_id / 4 & 7)));
}

int IC_8088::top_bit() {
    return 8 * (i_w_ + 1);
}

int IC_8088::sign_of(int val) {
    return 1 & (i_w_ ? (int16_t)val : (int8_t)val) >> (top_bit() - 1);
}

void IC_8088::index_inc(int reg_id) {
    regs16()[reg_id] -= (2 * regs8()[FLAG_DF] - 1) * (i_w_ + 1);
}

uint8_t IC_8088::fetch_byte(int offset) {
    while (offset >= prefetch_len_ && prefetch_len_ < 8) {
        prefetch_[prefetch_len_] = bus_read_byte(prefetch_base_ + prefetch_len_);
        prefetch_len_++;
    }
    return prefetch_[offset];
}

void IC_8088::decode_rm_reg() {
    uint32_t tab = 4 * !i_mod_;

    if (i_mod_ < 3) {
        int seg_reg = seg_override_en_ ? seg_override_ : TABLE[tab + 3][i_rm_];
        int base_reg_idx = TABLE[tab][i_rm_];
        int idx_reg_idx = TABLE[tab + 1][i_rm_];
        int disp_mult = TABLE[tab + 2][i_rm_];
        uint16_t offset = (uint16_t)(regs16()[idx_reg_idx]
                          + disp_mult * (int16_t)i_data1_
                          + regs16()[base_reg_idx]);
        op_to_addr_ = rm_addr_ = 16u * regs16()[seg_reg] + offset;
    } else {
        op_to_addr_ = rm_addr_ = get_reg_addr(i_rm_);
    }
    op_from_addr_ = get_reg_addr(i_reg_);

    if (i_d_) {
        uint32_t tmp = op_from_addr_;
        op_from_addr_ = rm_addr_;
        op_to_addr_ = tmp;
    }
}

// ========================================================================
// CPU core
// ========================================================================

void IC_8088::cpu_reset() {
    std::memset(regs_, 0, sizeof(regs_));
    regs16()[REG_CS] = start_cs_;
    reg_ip_ = start_ip_;
    seg_override_en_ = 0;
    rep_override_en_ = 0;
    trap_flag_ = 0;
    nmi_pending_ = false;
}

int IC_8088::set_CF(int new_CF) { return regs8()[FLAG_CF] = !!new_CF; }
int IC_8088::set_AF(int new_AF) { return regs8()[FLAG_AF] = !!new_AF; }
int IC_8088::set_OF(int new_OF) { return regs8()[FLAG_OF] = !!new_OF; }

void IC_8088::set_AF_OF_arith() {
    set_AF((op_source_ ^= op_dest_ ^ op_result_) & 0x10);
    if (op_result_ == (int)op_dest_)
        set_OF(0);
    else
        set_OF(1 & (regs8()[FLAG_CF] ^ op_source_ >> (top_bit() - 1)));
}

void IC_8088::make_flags() {
    scratch_uint_ = 0xF002;
    for (int i = 8; i >= 0; --i)
        scratch_uint_ += regs8()[FLAG_CF + i] << TABLE[TABLE_FLAGS_BITFIELDS][i];
}

void IC_8088::set_flags(int new_flags) {
    for (int i = 8; i >= 0; --i)
        regs8()[FLAG_CF + i] = !!(1 << TABLE[TABLE_FLAGS_BITFIELDS][i] & new_flags);
}

void IC_8088::set_opcode(uint8_t opcode) {
    xlat_opcode_id_ = TABLE[TABLE_XLAT_OPCODE][raw_opcode_id_ = opcode];
    extra_ = TABLE[TABLE_XLAT_SUBFUNCTION][opcode];
    i_mod_size_ = TABLE[TABLE_I_MOD_SIZE][opcode];
    set_flags_type_ = TABLE[TABLE_STD_FLAGS][opcode];
}

void IC_8088::pc_interrupt(uint8_t interrupt_num) {
    set_opcode(0xCD);
    make_flags();
    push16((uint16_t)scratch_uint_);
    push16(regs16()[REG_CS]);
    push16(reg_ip_);
    regs16()[REG_CS] = bus_read_word(4 * interrupt_num + 2);
    reg_ip_ = bus_read_word(4 * interrupt_num);
    regs8()[FLAG_TF] = 0;
    regs8()[FLAG_IF] = 0;
}

int IC_8088::AAA_AAS(int which_operation) {
    int adj = ((regs8()[REG_AL] & 0x0F) > 9) || regs8()[FLAG_AF];
    set_AF(adj); set_CF(adj);
    if (adj) regs16()[REG_AX] += 262 * which_operation;
    regs8()[REG_AL] &= 0x0F;
    return regs8()[REG_AL];
}

// ========================================================================
// Instruction execution -- ported from 8086tiny
// ========================================================================

void IC_8088::execute() {
    uint32_t cs_ip = 16u * regs16()[REG_CS] + reg_ip_;
    if (cs_ip == 0) return; // CS:IP = 0:0 = halt convention

    // Reset prefetch
    prefetch_base_ = cs_ip;
    prefetch_len_ = 0;

    set_opcode(fetch_byte(0));
    i_w_ = (i_reg4bit_ = raw_opcode_id_ & 7) & 1;
    i_d_ = i_reg4bit_ / 2 & 1;

    // Set up i_data fields from instruction stream
    i_data0_ = fetch_byte(1) | (fetch_byte(2) << 8);
    i_data1_ = fetch_byte(2) | (fetch_byte(3) << 8);
    i_data2_ = fetch_byte(3) | (fetch_byte(4) << 8);

    if (seg_override_en_) seg_override_en_--;
    if (rep_override_en_) rep_override_en_--;

    if (i_mod_size_) {
        i_mod_ = (i_data0_ & 0xFF) >> 6;
        i_rm_ = i_data0_ & 7;
        i_reg_ = (i_data0_ >> 3) & 7;

        if ((!i_mod_ && i_rm_ == 6) || (i_mod_ == 2))
            i_data2_ = fetch_byte(4) | (fetch_byte(5) << 8);
        else if (i_mod_ != 1)
            i_data2_ = i_data1_;
        else
            i_data1_ = (int8_t)(i_data0_ >> 8);

        decode_rm_reg();
    }

    // --- Instruction execution ---
    switch (xlat_opcode_id_) {
    case 0: { // Conditional jump (Jcc)
        scratch_uchar_ = raw_opcode_id_ / 2 & 7;
        reg_ip_ += (int8_t)(i_data0_ & 0xFF) * (i_w_ ^ (
            regs8()[TABLE[TABLE_COND_JUMP_DECODE_A][scratch_uchar_]] ||
            regs8()[TABLE[TABLE_COND_JUMP_DECODE_B][scratch_uchar_]] ||
            regs8()[TABLE[TABLE_COND_JUMP_DECODE_C][scratch_uchar_]] ^
            regs8()[TABLE[TABLE_COND_JUMP_DECODE_D][scratch_uchar_]]));
        break;
    }
    case 1: { // MOV reg, imm
        i_w_ = !!(raw_opcode_id_ & 8);
        uint32_t addr = get_reg_addr(i_reg4bit_);
        op_dest_ = rmem(addr);
        op_source_ = i_data0_;
        op_result_ = op_source_;
        wmem(addr, op_result_);
        break;
    }
    case 2: { // INC|DEC regs16
        i_w_ = 1;
        i_d_ = 0;
        i_reg_ = i_reg4bit_;
        decode_rm_reg();
        i_reg_ = extra_;
    }
    [[fallthrough]];
    case 5: { // INC|DEC|JMP|CALL|PUSH (group FF)
        if (i_reg_ < 2) {
            // INC (i_reg_=0) or DEC (i_reg_=1)
            uint32_t d = rmem(op_from_addr_);
            op_dest_ = d;
            op_source_ = 1;
            op_result_ = d + 1 - 2 * i_reg_;
            wmem(op_from_addr_, op_result_);
            set_AF_OF_arith();
            set_OF(op_dest_ + 1 - i_reg_ == (uint32_t)(1 << (top_bit() - 1)));
            if (xlat_opcode_id_ == 5) set_opcode(0x10); // decode like ADC for flags
        } else if (i_reg_ != 6) {
            // JMP or CALL (far/near)
            if (i_reg_ == 3) push16(regs16()[REG_CS]); // CALL far
            if (i_reg_ & 2) push16(reg_ip_ + 2 + i_mod_ * (i_mod_ != 3) + 2 * (!i_mod_ && i_rm_ == 6)); // CALL
            if (i_reg_ & 1) regs16()[REG_CS] = rmem16(op_from_addr_ + 2); // far
            reg_ip_ = rmem16(op_from_addr_);
            op_result_ = reg_ip_; // suppress flags
            set_opcode(0x9A);
        } else {
            // PUSH r/m
            i_w_ = 1;
            push16((uint16_t)rmem16(rm_addr_));
        }
        break;
    }
    case 6: { // TEST r/m,imm / NOT|NEG|MUL|IMUL|DIV|IDIV
        op_to_addr_ = op_from_addr_;
        switch (i_reg_) {
        case 0: { // TEST
            set_opcode(0x20); // decode like AND for flags
            reg_ip_ += i_w_ + 1;
            uint32_t d = rmem(op_to_addr_);
            uint32_t s = i_data2_;
            op_dest_ = d; op_source_ = s;
            op_result_ = i_w_ ? (uint16_t)(d & s) : (uint8_t)(d & s);
            break;
        }
        case 2: { // NOT
            uint32_t d = rmem(op_to_addr_);
            op_dest_ = d;
            op_result_ = ~d;
            wmem(op_to_addr_, op_result_);
            break;
        }
        case 3: { // NEG
            uint32_t d = rmem(op_to_addr_);
            op_dest_ = 0;
            op_source_ = d;
            op_result_ = -(int)d;
            wmem(op_to_addr_, op_result_);
            set_opcode(0x28); // decode like SUB for flags
            set_CF(op_result_ > op_dest_);
            break;
        }
        case 4: { // MUL
            uint32_t rm = rmem(rm_addr_);
            if (i_w_) {
                op_result_ = (unsigned short)rm * (unsigned short)regs16()[REG_AX];
                regs16()[REG_DX] = op_result_ >> 16;
                regs16()[REG_AX] = (uint16_t)op_result_;
                set_OF(set_CF(op_result_ - (uint16_t)op_result_));
            } else {
                op_result_ = (uint8_t)rm * (uint8_t)regs8()[REG_AL];
                regs16()[REG_AX] = (uint16_t)op_result_;
                set_OF(set_CF(op_result_ - (uint8_t)op_result_));
            }
            set_opcode(0x10);
            break;
        }
        case 5: { // IMUL
            uint32_t rm = rmem(rm_addr_);
            if (i_w_) {
                op_result_ = (short)rm * (short)regs16()[REG_AX];
                regs16()[REG_DX] = op_result_ >> 16;
                regs16()[REG_AX] = (uint16_t)op_result_;
                set_OF(set_CF(op_result_ - (short)op_result_));
            } else {
                op_result_ = (int8_t)(uint8_t)rm * (int8_t)regs8()[REG_AL];
                regs16()[REG_AX] = (uint16_t)op_result_;
                set_OF(set_CF(op_result_ - (int8_t)op_result_));
            }
            set_opcode(0x10);
            break;
        }
        case 6: { // DIV
            uint32_t rm = rmem(rm_addr_);
            if (i_w_) {
                scratch_int_ = (int)(unsigned short)rm;
                if (scratch_int_) {
                    scratch_uint_ = (regs16()[REG_DX] << 16) + regs16()[REG_AX];
                    scratch2_uint_ = scratch_uint_ / scratch_int_;
                    if (scratch2_uint_ == (uint16_t)scratch2_uint_) {
                        regs16()[REG_DX] = scratch_uint_ - scratch_int_ * scratch2_uint_;
                        regs16()[REG_AX] = (uint16_t)scratch2_uint_;
                    } else pc_interrupt(0);
                } else pc_interrupt(0);
            } else {
                scratch_int_ = (int)(uint8_t)rm;
                if (scratch_int_) {
                    scratch_uint_ = regs16()[REG_AX];
                    scratch2_uint_ = (uint16_t)scratch_uint_ / scratch_int_;
                    if (scratch2_uint_ == (uint8_t)scratch2_uint_) {
                        regs8()[REG_AH] = scratch_uint_ - scratch_int_ * scratch2_uint_;
                        regs8()[REG_AL] = (uint8_t)scratch2_uint_;
                    } else pc_interrupt(0);
                } else pc_interrupt(0);
            }
            break;
        }
        case 7: { // IDIV
            uint32_t rm = rmem(rm_addr_);
            if (i_w_) {
                scratch_int_ = (short)(uint16_t)rm;
                if (scratch_int_) {
                    scratch_uint_ = (regs16()[REG_DX] << 16) + regs16()[REG_AX];
                    scratch2_uint_ = (int)scratch_uint_ / scratch_int_;
                    if ((int)scratch2_uint_ == (short)scratch2_uint_) {
                        regs16()[REG_DX] = (int)scratch_uint_ - scratch_int_ * (int)scratch2_uint_;
                        regs16()[REG_AX] = (uint16_t)scratch2_uint_;
                    } else pc_interrupt(0);
                } else pc_interrupt(0);
            } else {
                scratch_int_ = (int8_t)(uint8_t)rm;
                if (scratch_int_) {
                    scratch_uint_ = regs16()[REG_AX];
                    scratch2_uint_ = (short)scratch_uint_ / scratch_int_;
                    if ((int)scratch2_uint_ == (int8_t)scratch2_uint_) {
                        regs8()[REG_AH] = (short)scratch_uint_ - scratch_int_ * (int)scratch2_uint_;
                        regs8()[REG_AL] = (uint8_t)scratch2_uint_;
                    } else pc_interrupt(0);
                } else pc_interrupt(0);
            }
            break;
        }
        default: break;
        }
        break;
    }
    case 7: { // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP AL/AX, imm
        rm_addr_ = REGS_BASE;
        i_data2_ = i_data0_;
        i_mod_ = 3;
        i_reg_ = extra_;
        reg_ip_--;
    }
    [[fallthrough]];
    case 8: { // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP reg, imm
        op_to_addr_ = rm_addr_;
        regs16()[REG_SCRATCH] = (i_d_ |= !i_w_) ? (int8_t)(i_data2_ & 0xFF) : (uint16_t)i_data2_;
        op_from_addr_ = REGS_BASE + 2 * REG_SCRATCH;
        reg_ip_ += !i_d_ + 1;
        set_opcode(0x08 * (extra_ = i_reg_));
    }
    [[fallthrough]];
    case 9: { // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP|MOV reg, r/m
        uint32_t d = rmem(op_to_addr_);
        uint32_t s = rmem(op_from_addr_);
        op_dest_ = d; op_source_ = s;
        switch (extra_) {
        case 0: // ADD
            op_result_ = d + s;
            wmem(op_to_addr_, op_result_);
            set_CF(i_w_ ? (uint16_t)op_result_ < (uint16_t)d : (uint8_t)op_result_ < (uint8_t)d);
            break;
        case 1: // OR
            op_result_ = d | s;
            wmem(op_to_addr_, op_result_);
            break;
        case 2: { // ADC
            int cf = regs8()[FLAG_CF];
            op_result_ = d + s + cf;
            wmem(op_to_addr_, op_result_);
            set_CF((cf && (uint32_t)op_result_ == d) ||
                   (i_w_ ? (uint16_t)op_result_ < (uint16_t)d : (uint8_t)op_result_ < (uint8_t)d));
            set_AF_OF_arith();
            break;
        }
        case 3: { // SBB
            int cf = regs8()[FLAG_CF];
            op_result_ = d - s - cf;
            wmem(op_to_addr_, op_result_);
            set_CF((cf && (uint32_t)op_result_ == d) ||
                   (i_w_ ? (uint16_t)d < (uint16_t)(s + cf) : (uint8_t)d < (uint8_t)(s + cf)));
            set_AF_OF_arith();
            break;
        }
        case 4: // AND
            op_result_ = d & s;
            wmem(op_to_addr_, op_result_);
            break;
        case 5: // SUB
            op_result_ = d - s;
            wmem(op_to_addr_, op_result_);
            set_CF(i_w_ ? (uint16_t)d < (uint16_t)s : (uint8_t)d < (uint8_t)s);
            break;
        case 6: // XOR
            op_result_ = d ^ s;
            wmem(op_to_addr_, op_result_);
            break;
        case 7: // CMP
            op_result_ = d - s;
            // no writeback
            set_CF(i_w_ ? (uint16_t)d < (uint16_t)s : (uint8_t)d < (uint8_t)s);
            break;
        case 8: // MOV
            op_result_ = s;
            wmem(op_to_addr_, op_result_);
            break;
        }
        break;
    }
    case 10: { // MOV sreg,r/m | POP r/m | LEA reg,r/m
        if (!i_w_) { // MOV sreg
            i_w_ = 1;
            i_reg_ += 8;
            decode_rm_reg();
            op_result_ = rmem(op_from_addr_);
            wmem(op_to_addr_, op_result_);
        } else if (!i_d_) { // LEA
            seg_override_en_ = 1;
            seg_override_ = REG_ZERO;
            decode_rm_reg();
            wmem16(op_from_addr_, rm_addr_);
            op_result_ = rm_addr_;
        } else { // POP r/m
            i_w_ = 1;
            uint16_t val = pop16();
            wmem16(rm_addr_, val);
            op_result_ = val;
        }
        break;
    }
    case 11: { // MOV AL/AX, [loc]
        i_mod_ = 0; i_reg_ = 0; i_rm_ = 6;
        i_data1_ = i_data0_;
        decode_rm_reg();
        op_result_ = rmem(op_to_addr_);
        wmem(op_from_addr_, op_result_);
        break;
    }
    case 12: { // ROL|ROR|RCL|RCR|SHL|SHR|???|SAR reg/mem, 1/CL/imm
        uint32_t val = rmem(rm_addr_);
        scratch2_uint_ = sign_of(val);
        scratch_uint_ = extra_ ? (++reg_ip_, (int8_t)(i_data1_ & 0xFF))
                               : i_d_ ? (31 & regs8()[REG_CL])
                                       : 1;
        if (scratch_uint_) {
            if (i_reg_ < 4) {
                scratch_uint_ %= i_reg_ / 2 + top_bit();
                scratch2_uint_ = rmem(rm_addr_);
            }
            if (i_reg_ & 1) {
                // Right shift/rotate
                op_dest_ = rmem(rm_addr_); op_source_ = scratch_uint_;
                op_result_ = i_w_ ? (uint16_t)op_dest_ >> scratch_uint_ : (uint8_t)op_dest_ >> scratch_uint_;
                wmem(rm_addr_, op_result_);
            } else {
                // Left shift/rotate
                op_dest_ = rmem(rm_addr_); op_source_ = scratch_uint_;
                op_result_ = op_dest_ << scratch_uint_;
                wmem(rm_addr_, op_result_);
            }
            if (i_reg_ > 3) set_opcode(0x10); // shift: decode like ADC for flags
            if (i_reg_ > 4) set_CF(op_dest_ >> (scratch_uint_ - 1) & 1); // SHR or SAR
        }
        switch (i_reg_) {
        case 0: { // ROL
            uint32_t combined = rmem(rm_addr_);
            combined += scratch2_uint_ >> (top_bit() - scratch_uint_);
            op_result_ = combined;
            wmem(rm_addr_, op_result_);
            set_OF(sign_of(op_result_) ^ set_CF(op_result_ & 1));
            break;
        }
        case 1: { // ROR
            scratch2_uint_ &= (1 << scratch_uint_) - 1;
            uint32_t combined = rmem(rm_addr_);
            combined += scratch2_uint_ << (top_bit() - scratch_uint_);
            op_result_ = combined;
            wmem(rm_addr_, op_result_);
            set_OF(sign_of(op_result_ * 2) ^ set_CF(sign_of(op_result_)));
            break;
        }
        case 2: { // RCL
            uint32_t combined = rmem(rm_addr_);
            combined += (regs8()[FLAG_CF] << (scratch_uint_ - 1))
                      + (scratch2_uint_ >> (1 + top_bit() - scratch_uint_));
            op_result_ = combined;
            wmem(rm_addr_, op_result_);
            set_OF(sign_of(op_result_) ^ set_CF(scratch2_uint_ & (1 << (top_bit() - scratch_uint_))));
            break;
        }
        case 3: { // RCR
            uint32_t combined = rmem(rm_addr_);
            combined += (regs8()[FLAG_CF] << (top_bit() - scratch_uint_))
                      + (scratch2_uint_ << (1 + top_bit() - scratch_uint_));
            op_result_ = combined;
            wmem(rm_addr_, op_result_);
            set_CF(scratch2_uint_ & (1 << (scratch_uint_ - 1)));
            set_OF(sign_of(op_result_) ^ sign_of(op_result_ * 2));
            break;
        }
        case 4: // SHL
            set_OF(sign_of(op_result_) ^ set_CF(sign_of(op_dest_ << (scratch_uint_ - 1))));
            break;
        case 5: // SHR
            set_OF(sign_of(op_dest_));
            break;
        case 7: { // SAR
            if (scratch_uint_ < (uint32_t)top_bit()) { /* nothing */ } else set_CF(scratch2_uint_);
            set_OF(0);
            // Fill sign bits
            uint32_t fill = scratch2_uint_ * ~(((1u << top_bit()) - 1) >> scratch_uint_);
            uint32_t combined = rmem(rm_addr_);
            combined += fill;
            op_result_ = combined;
            wmem(rm_addr_, op_result_);
            break;
        }
        default: break;
        }
        break;
    }
    case 13: { // LOOPxx|JCXZ
        scratch_uint_ = !!--regs16()[REG_CX];
        switch (i_reg4bit_) {
        case 0: scratch_uint_ &= !regs8()[FLAG_ZF]; break; // LOOPNZ
        case 1: scratch_uint_ &= regs8()[FLAG_ZF]; break;  // LOOPZ
        case 3: scratch_uint_ = !++regs16()[REG_CX]; break; // JCXZ
        }
        reg_ip_ += scratch_uint_ * (int8_t)(i_data0_ & 0xFF);
        break;
    }
    case 14: { // JMP | CALL short/near/far
        reg_ip_ += 3 - i_d_;
        if (!i_w_) {
            if (i_d_) { // JMP far
                reg_ip_ = 0;
                regs16()[REG_CS] = (uint16_t)i_data2_;
            } else { // CALL near
                push16(reg_ip_);
            }
        }
        reg_ip_ += (i_d_ && i_w_) ? (int8_t)(i_data0_ & 0xFF) : (int16_t)i_data0_;
        break;
    }
    case 15: { // TEST reg, r/m
        uint32_t d = rmem(op_from_addr_);
        uint32_t s = rmem(op_to_addr_);
        op_dest_ = d; op_source_ = s;
        op_result_ = d & s;
        break;
    }
    case 16: { // XCHG AX, regs16
        i_w_ = 1;
        op_to_addr_ = REGS_BASE;
        op_from_addr_ = get_reg_addr(i_reg4bit_);
    }
    [[fallthrough]];
    case 24: { // NOP|XCHG reg, r/m
        if (op_to_addr_ != op_from_addr_) {
            uint32_t a = rmem(op_to_addr_);
            uint32_t b = rmem(op_from_addr_);
            wmem(op_to_addr_, b);
            wmem(op_from_addr_, a);
            op_result_ = b;
        }
        break;
    }
    case 17: { // MOVSx (extra=0)|STOSx (extra=1)|LODSx (extra=2)
        scratch2_uint_ = seg_override_en_ ? seg_override_ : REG_DS;
        for (scratch_uint_ = rep_override_en_ ? regs16()[REG_CX] : 1;
             scratch_uint_; scratch_uint_--) {
            uint32_t src_addr = (extra_ & 1)
                ? REGS_BASE  // STOS: source is AX/AL
                : 16u * regs16()[scratch2_uint_] + regs16()[REG_SI]; // MOVS/LODS
            uint32_t dst_addr = (extra_ < 2)
                ? 16u * regs16()[REG_ES] + regs16()[REG_DI]  // MOVS/STOS
                : REGS_BASE;  // LODS: dest is AX/AL
            uint32_t val = rmem(src_addr);
            op_result_ = val;
            wmem(dst_addr, val);
            if (!(extra_ & 1)) index_inc(REG_SI);
            if (!(extra_ & 2)) index_inc(REG_DI);
        }
        if (rep_override_en_) regs16()[REG_CX] = 0;
        break;
    }
    case 18: { // CMPSx (extra=0)|SCASx (extra=1)
        scratch2_uint_ = seg_override_en_ ? seg_override_ : REG_DS;
        scratch_uint_ = rep_override_en_ ? regs16()[REG_CX] : 1;
        if (scratch_uint_) {
            for (; scratch_uint_; rep_override_en_ || scratch_uint_--) {
                uint32_t src_addr = extra_
                    ? REGS_BASE  // SCAS: compare with AX/AL
                    : 16u * regs16()[scratch2_uint_] + regs16()[REG_SI];
                uint32_t cmp_addr = 16u * regs16()[REG_ES] + regs16()[REG_DI];
                uint32_t d = rmem(src_addr);
                uint32_t s = rmem(cmp_addr);
                op_dest_ = d; op_source_ = s;
                op_result_ = d - s;
                if (!extra_) index_inc(REG_SI);
                index_inc(REG_DI);
                if (rep_override_en_ && !(--regs16()[REG_CX] && (!op_result_ == rep_mode_)))
                    scratch_uint_ = 0;
            }
            set_flags_type_ = FLAGS_UPDATE_SZP | FLAGS_UPDATE_AO_ARITH;
            set_CF(i_w_ ? (uint16_t)op_dest_ < (uint16_t)op_source_
                        : (uint8_t)op_dest_ < (uint8_t)op_source_);
        }
        break;
    }
    case 19: { // RET|RETF|IRET
        i_d_ = i_w_;
        reg_ip_ = pop16();
        if (extra_) regs16()[REG_CS] = pop16(); // RETF or IRET
        if (extra_ & 2) set_flags(pop16()); // IRET
        else if (!i_d_) regs16()[REG_SP] += (uint16_t)i_data0_; // RET/RETF imm16
        break;
    }
    case 20: { // MOV r/m, imm
        op_result_ = i_data2_;
        wmem(op_from_addr_, op_result_);
        break;
    }
    case 21: { // IN AL/AX, DX/imm8
        scratch_uint_ = extra_ ? regs16()[REG_DX] : (uint8_t)i_data0_;
        uint8_t val = io_read_byte((uint16_t)scratch_uint_);
        regs8()[REG_AL] = val;
        if (i_w_) regs8()[REG_AH] = io_read_byte((uint16_t)(scratch_uint_ + 1));
        op_result_ = regs8()[REG_AL];
        break;
    }
    case 22: { // OUT DX/imm8, AL/AX
        scratch_uint_ = extra_ ? regs16()[REG_DX] : (uint8_t)i_data0_;
        io_write_byte((uint16_t)scratch_uint_, regs8()[REG_AL]);
        if (i_w_) io_write_byte((uint16_t)(scratch_uint_ + 1), regs8()[REG_AH]);
        break;
    }
    case 23: { // REPxx
        rep_override_en_ = 2;
        rep_mode_ = i_w_;
        if (seg_override_en_) seg_override_en_++;
        break;
    }
    case 25: { // PUSH reg
        push16(regs16()[extra_]);
        break;
    }
    case 26: { // POP reg
        regs16()[extra_] = pop16();
        break;
    }
    case 27: { // Segment override
        seg_override_en_ = 2;
        seg_override_ = extra_;
        if (rep_override_en_) rep_override_en_++;
        break;
    }
    case 28: { // DAA/DAS
        i_w_ = 0;
        scratch2_uint_ = regs8()[REG_AL];
        if (extra_) {
            // DAS
            if (((scratch2_uint_ & 0x0F) > 9) || regs8()[FLAG_AF]) {
                op_result_ = regs8()[REG_AL] -= 6;
                set_CF(regs8()[FLAG_CF] || (regs8()[REG_AL] >= scratch2_uint_));
            }
            set_AF(((scratch2_uint_ & 0x0F) > 9) || regs8()[FLAG_AF]);
            if (((0xFF & scratch2_uint_) > 0x99) || regs8()[FLAG_CF]) {
                op_result_ = regs8()[REG_AL] -= 0x60;
                set_CF(1);
            }
        } else {
            // DAA
            if (((scratch2_uint_ & 0x0F) > 9) || regs8()[FLAG_AF]) {
                op_result_ = regs8()[REG_AL] += 6;
                set_CF(regs8()[FLAG_CF] || (regs8()[REG_AL] < scratch2_uint_));
            }
            set_AF(((scratch2_uint_ & 0x0F) > 9) || regs8()[FLAG_AF]);
            if (((0xF0 & scratch2_uint_) > 0x90) || regs8()[FLAG_CF]) {
                op_result_ = regs8()[REG_AL] += 0x60;
                set_CF(1);
            }
        }
        op_result_ = regs8()[REG_AL];
        break;
    }
    case 29: // AAA/AAS
        op_result_ = AAA_AAS(extra_ - 1);
        break;
    case 30: // CBW
        regs8()[REG_AH] = -(regs8()[REG_AL] >> 7);
        break;
    case 31: // CWD
        regs16()[REG_DX] = -(regs16()[REG_AX] >> 15);
        break;
    case 32: // CALL FAR imm16:imm16
        push16(regs16()[REG_CS]);
        push16(reg_ip_ + 5);
        regs16()[REG_CS] = (uint16_t)i_data2_;
        reg_ip_ = (uint16_t)i_data0_;
        break;
    case 33: // PUSHF
        make_flags();
        push16((uint16_t)scratch_uint_);
        break;
    case 34: // POPF
        set_flags(pop16());
        break;
    case 35: // SAHF
        make_flags();
        set_flags((scratch_uint_ & 0xFF00) + regs8()[REG_AH]);
        break;
    case 36: // LAHF
        make_flags();
        regs8()[REG_AH] = (uint8_t)scratch_uint_;
        break;
    case 37: { // LES|LDS reg, r/m
        i_w_ = i_d_ = 1;
        decode_rm_reg();
        uint32_t val = rmem16(op_from_addr_);
        wmem16(op_to_addr_, val);
        wmem16(REGS_BASE + extra_, rmem16(rm_addr_ + 2));
        op_result_ = val;
        break;
    }
    case 38: // INT 3
        ++reg_ip_;
        pc_interrupt(3);
        break;
    case 39: // INT imm8
        reg_ip_ += 2;
        pc_interrupt((uint8_t)i_data0_);
        break;
    case 40: // INTO
        ++reg_ip_;
        if (regs8()[FLAG_OF]) pc_interrupt(4);
        break;
    case 41: { // AAM
        uint8_t divisor = (uint8_t)(i_data0_ & 0xFF);
        if (divisor) {
            regs8()[REG_AH] = regs8()[REG_AL] / divisor;
            op_result_ = regs8()[REG_AL] %= divisor;
        } else {
            pc_interrupt(0);
        }
        break;
    }
    case 42: // AAD
        i_w_ = 0;
        regs16()[REG_AX] = op_result_ = 0xFF & (regs8()[REG_AL] + (uint8_t)i_data0_ * regs8()[REG_AH]);
        break;
    case 43: // SALC
        regs8()[REG_AL] = -regs8()[FLAG_CF];
        break;
    case 44: { // XLAT
        uint32_t seg = seg_override_en_ ? seg_override_ : REG_DS;
        regs8()[REG_AL] = rmem8(16u * regs16()[seg] + (uint16_t)(regs16()[REG_BX] + regs8()[REG_AL]));
        break;
    }
    case 45: // CMC
        regs8()[FLAG_CF] ^= 1;
        break;
    case 46: // CLC|STC|CLI|STI|CLD|STD
        regs8()[extra_ / 2] = extra_ & 1;
        break;
    case 47: { // TEST AL/AX, imm
        uint32_t d = i_w_ ? regs16()[REG_AX] : regs8()[REG_AL];
        uint32_t s = i_data0_;
        op_dest_ = d; op_source_ = s;
        op_result_ = d & s;
        break;
    }
    case 48: // 0F xx (emulator-specific, not used on real hardware)
        break;

    case 3: // PUSH regs16
        push16(regs16()[i_reg4bit_]);
        break;
    case 4: // POP regs16
        regs16()[i_reg4bit_] = pop16();
        break;
    default:
        break;
    }

    // Advance IP by computed instruction length
    reg_ip_ += (i_mod_ * (i_mod_ != 3) + 2 * (!i_mod_ && i_rm_ == 6)) * i_mod_size_
             + TABLE[TABLE_BASE_INST_SIZE][raw_opcode_id_]
             + TABLE[TABLE_I_W_SIZE][raw_opcode_id_] * (i_w_ + 1);

    // Update SZP flags
    if (set_flags_type_ & FLAGS_UPDATE_SZP) {
        regs8()[FLAG_SF] = sign_of(op_result_);
        regs8()[FLAG_ZF] = !(i_w_ ? (uint16_t)op_result_ : (uint8_t)op_result_);
        regs8()[FLAG_PF] = TABLE[TABLE_PARITY_FLAG][(uint8_t)op_result_];
        if (set_flags_type_ & FLAGS_UPDATE_AO_ARITH) set_AF_OF_arith();
        if (set_flags_type_ & FLAGS_UPDATE_OC_LOGIC) { set_CF(0); set_OF(0); }
    }

    // Trap flag
    if (trap_flag_) pc_interrupt(1);
    trap_flag_ = regs8()[FLAG_TF];

    // Interrupt check
    if (regs8()[FLAG_IF] && !seg_override_en_ && !rep_override_en_ && !regs8()[FLAG_TF]) {
        if (nmi_pending_) {
            nmi_pending_ = false;
            pc_interrupt(2);
        } else if (pin_intr_ && pin_intr_->level() == Level::High) {
            // INTA bus cycle: drive INTA status, read vector from 8259A
            drive_status((BUS_INTA >> 2) & 1, (BUS_INTA >> 1) & 1, BUS_INTA & 1);
            wait_clk_rising(); wait_clk_falling();
            wait_clk_rising(); wait_clk_falling();
            drive_status_passive();
            // Second INTA pulse: read vector byte
            drive_status((BUS_INTA >> 2) & 1, (BUS_INTA >> 1) & 1, BUS_INTA & 1);
            wait_clk_rising(); wait_clk_falling();
            release_data();
            wait_clk_rising(); wait_clk_falling();
            wait_clk_rising(); wait_clk_falling();
            uint8_t vector = read_data();
            drive_status_passive();
            wait_clk_rising(); wait_clk_falling();
            pc_interrupt(vector);
        }
    }
}

} // namespace bench
