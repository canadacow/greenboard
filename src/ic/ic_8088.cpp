#include "ic/ic_8088.h"
#include "core/scheduler.h"
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
    : FiberComponent("8088"), start_cs_(start_cs), start_ip_(start_ip) { set_description("CPU"); }

void IC_8088::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    for (int i = 0; i < 8; ++i)
        pin_ad_[i] = pin(16 - i);
    for (int i = 0; i < 7; ++i)
        pin_a_upper_[i] = pin(8 - i);
    for (int i = 0; i < 5; ++i)
        pin_a_upper_[7 + i] = pin(39 - i);

    pin_s0_ = pin(26);
    pin_s1_ = pin(27);
    pin_s2_ = pin(28);
    pin_qs0_ = pin(25);
    pin_qs1_ = pin(24);
    pin_clk_   = connect_pin(19);
    pin_reset_ = connect_pin(21);
    pin_ready_ = pin(22);
    pin_intr_  = pin(18);
    pin_nmi_   = connect_pin(17);
    pin_test_  = pin(23);
    pin_vcc_   = connect_pin(31);
    pin_lock_  = pin(29);
    pin_rqgt0_ = pin(30);

    // Pin directions for wiring visualization.
    for (int i = 0; i < 8; ++i) { declare_input(pin_ad_[i]); declare_output(pin_ad_[i]); }
    for (int i = 0; i < 12; ++i) declare_output(pin_a_upper_[i]);
    declare_output(pin_s0_); declare_output(pin_s1_); declare_output(pin_s2_);
    declare_output(pin_qs0_); declare_output(pin_qs1_);
    declare_output(pin_lock_);
    declare_input(pin_clk_); declare_input(pin_reset_);
    declare_async_input(pin_ready_); declare_async_input(pin_intr_);
    declare_async_input(pin_nmi_);   declare_async_input(pin_test_);

    // AD0-AD7 are bidirectional: output during T1 (address) and write data,
    // input during read data.
    declare_bidir_block({pin_ad_[0], pin_ad_[1], pin_ad_[2], pin_ad_[3],
                         pin_ad_[4], pin_ad_[5], pin_ad_[6], pin_ad_[7]},
                        [this]() {
                            return (bus_t_ == BusT::T1 || bus_t_ == BusT::T2_Write)
                                   ? BidirDir::Output : BidirDir::Input;
                        });

    // S0-S2 + ~LOCK: Output unless AD is in Input mode (T2_Read), where they
    // vanish from the DAG to break the 8088->8288->...->8088 cycle.
    // ~LOCK is always High during normal bus cycles; including it here breaks
    // the U3->U5->...->DRAM->U12->U8->U3 cycle in the read-phase perm.
    declare_bidir_block({pin_s0_, pin_s1_, pin_s2_, pin_lock_},
                        BidirDir::Output | BidirDir::HiZ,
                        [this]() {
                            return bus_t_ == BusT::T2_Read ? BidirDir::HiZ : BidirDir::Output;
                        });
}

void IC_8088::check_nmi() {
    Level cur = pin_nmi_.level();
    if (cur == Level::High && nmi_prev_ != Level::High)
        nmi_pending_ = true;
    nmi_prev_ = cur;
}

void IC_8088::run() {
    // Wait for VCC.
    while (pin_vcc_.level() != Level::High)
        yield();
    spdlog::info("[8088] VCC detected, waiting for RESET");

    // Sample RESET on CLK edges, like the real chip.
    // Stay in reset while RESET is High. Start executing when it goes Low.
    // If we missed the pulse entirely (RESET already Low), proceed immediately.
    cpu_reset();

    if (pin_reset_.level() == Level::High) {
        // RESET is currently asserted -- wait for it to deassert.
        spdlog::info("[8088] RESET asserted -- CS:IP = {:04X}:{:04X}", start_cs_, start_ip_);
        while (pin_reset_.level() == Level::High)
            yield();
    } else {
        // RESET pulse already completed (or never happened).
        spdlog::info("[8088] RESET complete -- CS:IP = {:04X}:{:04X}", start_cs_, start_ip_);
    }

    spdlog::info("[8088] starting execution");

    drive_status_passive();

    for (;;) {
        if (pin_vcc_.level() != Level::High) break;
        check_nmi();  // process NMI
        if (halted_) {
            yield();
            continue;
        }
        execute();
    }

    drive_status_passive();
    release_data();
    for (int i = 0; i < 12; ++i)
        pin_a_upper_[i].release();
    pin_lock_.release();
    pin_qs0_.release();
    pin_qs1_.release();
}

// ========================================================================
// Bus operations
// ========================================================================

void IC_8088::drive_address(uint32_t address) {
    bus_t_ = BusT::T1;
    for (int i = 0; i < 8; ++i)
        pin_ad_[i].drive((address >> i) & 1 ? Level::High : Level::Low);
    for (int i = 0; i < 12; ++i)
        pin_a_upper_[i].drive((address >> (i + 8)) & 1 ? Level::High : Level::Low);
    spdlog::trace("[8088] drive_address 0x{:05X} AD=0x{:02X} A8-15=0x{:02X} A16-19=0x{:01X} CS={:04X} DS={:04X} ES={:04X} SS={:04X}",
                  address, address & 0xFF, (address >> 8) & 0xFF, (address >> 16) & 0xF,
                  regs16()[REG_CS], regs16()[REG_DS], regs16()[REG_ES], regs16()[REG_SS]);
    spdlog::trace("[8088]   AD pool after drive: {}={} {}={} {}={} {}={} {}={} {}={} {}={} {}={}",
                  pin_ad_[0].idx, int(SignalPool::levels[pin_ad_[0].idx]),
                  pin_ad_[1].idx, int(SignalPool::levels[pin_ad_[1].idx]),
                  pin_ad_[2].idx, int(SignalPool::levels[pin_ad_[2].idx]),
                  pin_ad_[3].idx, int(SignalPool::levels[pin_ad_[3].idx]),
                  pin_ad_[4].idx, int(SignalPool::levels[pin_ad_[4].idx]),
                  pin_ad_[5].idx, int(SignalPool::levels[pin_ad_[5].idx]),
                  pin_ad_[6].idx, int(SignalPool::levels[pin_ad_[6].idx]),
                  pin_ad_[7].idx, int(SignalPool::levels[pin_ad_[7].idx]));
}

void IC_8088::drive_data(uint8_t value) {
    bus_t_ = BusT::T2_Write;
    for (int i = 0; i < 8; ++i)
        pin_ad_[i].drive((value >> i) & 1 ? Level::High : Level::Low);

    spdlog::trace("[8088] drive_data 0x{:02X}", value);        
}

uint8_t IC_8088::read_data() {
    bus_t_ = BusT::T2_Read;
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i)
        if (pin_ad_[i].level() == Level::High)
            val |= (1 << i);
    spdlog::trace("[8088] read_data 0x{:02X}", val);
    return val;
}

void IC_8088::release_data() {
    bus_t_ = BusT::T2_Read;
    for (int i = 0; i < 8; ++i)
        pin_ad_[i].release();
}

void IC_8088::drive_status(uint8_t s2, uint8_t s1, uint8_t s0) {
    pin_s0_.drive(s0 ? Level::High : Level::Low);
    pin_s1_.drive(s1 ? Level::High : Level::Low);
    pin_s2_.drive(s2 ? Level::High : Level::Low);
}

void IC_8088::drive_status_passive() {
    pin_s0_.drive(Level::High);
    pin_s1_.drive(Level::High);
    pin_s2_.drive(Level::High);
    pin_lock_.drive(Level::High);  // ~LOCK: active-low, deasserted during normal operation
}

void IC_8088::full_wait_clk(const char* stateYield) {
    spdlog::trace("[8088] T-state {}", stateYield);
    yield();
    check_nmi();
}


// ---- Memory read: 4 T-states (T1, T2, T3, T4) + optional Tw ----
uint8_t IC_8088::bus_read_byte(uint32_t address) {
    // T1 -- drive S0-S2 (MEMR), drive address on AD0-AD7 / A8-A19
    drive_status((BUS_MEMR >> 2) & 1, (BUS_MEMR >> 1) & 1, BUS_MEMR & 1);
    drive_address(address & 0xFFFFF);
    full_wait_clk("T1 bus_read");                                // T1

    // T2 -- ALE falls, latches capture. Release AD. Status stays active.
    release_data();
    full_wait_clk("T2 bus_read");                                // T2

    // T3 -- status goes passive. Data driven by memory/peripherals.
    drive_status_passive();
    full_wait_clk("T3 bus_read");                                // T3

    // Tw -- wait states while READY is low
    while (pin_ready_.level() != Level::High) {
        spdlog::trace("[8088] Tw wait (read) READY={}", int(pin_ready_.level()));
        full_wait_clk("Tw bus_read");                            // Tw
    }
    
    // T4 -- bus cycle complete
    uint8_t data = read_data();
    bus_t_ = BusT::T1;  // next perm sees S0-S2 as Output for upcoming T1
    full_wait_clk("T4 bus_read");                                // T4
    return data;
}

// ---- Memory write: 4 T-states (T1, T2, T3, T4) + optional Tw ----
void IC_8088::bus_write_byte(uint32_t address, uint8_t value) {
    // T1 -- drive S0-S2 (MEMW), drive address on AD0-AD7 / A8-A19
    drive_status((BUS_MEMW >> 2) & 1, (BUS_MEMW >> 1) & 1, BUS_MEMW & 1);
    drive_address(address & 0xFFFFF);
    full_wait_clk("T1 bus_write");                               // T1

    // T2 -- ALE falls, latches capture. Switch AD to write data. Status stays active.
    drive_data(value);
    full_wait_clk("T2 bus_write");                               // T2

    // T3 -- status goes passive. Data held on bus.
    drive_status_passive();
    full_wait_clk("T3 bus_write");                               // T3

    // Tw -- wait states while READY is low
    while (pin_ready_.level() != Level::High) {
        spdlog::trace("[8088] Tw wait (write) READY={}", int(pin_ready_.level()));
        full_wait_clk("Tw bus_write");                           // Tw
    }

    // T4 -- bus cycle complete, release data bus
    release_data();
    bus_t_ = BusT::T1;  // next perm sees S0-S2 as Output for upcoming T1
    full_wait_clk("T4 bus_write");                               // T4
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

// ---- I/O read: 4 T-states (T1, T2, T3, T4) + optional Tw ----
uint8_t IC_8088::io_read_byte(uint16_t port) {
    // T1 -- drive S0-S2 (IOR), drive port address
    drive_status((BUS_IOR >> 2) & 1, (BUS_IOR >> 1) & 1, BUS_IOR & 1);
    drive_address(port);
    full_wait_clk("T1 io_read");                                 // T1

    // T2 -- ALE falls, latches capture. Release AD. Status stays active.
    release_data();
    full_wait_clk("T2 io_read");                                 // T2

    // T3 -- status goes passive. Data driven by peripheral.
    drive_status_passive();
    full_wait_clk("T3 io_read");                                 // T3

    // Tw -- wait states while READY is low
    while (pin_ready_.level() != Level::High) {
        spdlog::trace("[8088] Tw wait (io_read) READY={}", int(pin_ready_.level()));
        full_wait_clk("Tw io_read");                             // Tw
    }

    // T4 -- bus cycle complete
    uint8_t data = read_data();
    bus_t_ = BusT::T1;  // next perm sees S0-S2 as Output for upcoming T1
    full_wait_clk("T4 io_read");                                 // T4
    return data;
}

// ---- I/O write: 4 T-states (T1, T2, T3, T4) + optional Tw ----
void IC_8088::io_write_byte(uint16_t port, uint8_t value) {
    // T1 -- drive S0-S2 (IOW), drive port address
    drive_status((BUS_IOW >> 2) & 1, (BUS_IOW >> 1) & 1, BUS_IOW & 1);
    drive_address(port);
    full_wait_clk("T1 io_write");                                // T1

    // T2 -- ALE falls, latches capture. Switch AD to write data. Status stays active.
    drive_data(value);
    full_wait_clk("T2 io_write");                                // T2

    // T3 -- status goes passive. Data held on bus.
    drive_status_passive();
    full_wait_clk("T3 io_write");                                // T3

    // Tw -- wait states while READY is low
    while (pin_ready_.level() != Level::High) {
        spdlog::trace("[8088] Tw wait (write) READY={}", int(pin_ready_.level()));
        full_wait_clk("Tw io_write");                            // Tw
    }    

    // T4 -- bus cycle complete, release data bus
    release_data();
    bus_t_ = BusT::T1;  // next perm sees S0-S2 as Output for upcoming T1
    full_wait_clk("T4 io_write");                                // T4
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
    return REGS_BASE + (i_w_ ? 2 * reg_id : ((2 * reg_id + reg_id / 4) & 7));
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
#if 0 // This is busted
    while (offset >= prefetch_len_ && prefetch_len_ < 4) {
        prefetch_[prefetch_len_] = bus_read_byte(prefetch_base_ + prefetch_len_);
        prefetch_len_++;
    }
    return prefetch_[offset];
#else
    return bus_read_byte(prefetch_base_ + offset);
#endif
}

uint16_t IC_8088::fetch_word(int offset) {
    return fetch_byte(offset) | (fetch_byte(offset + 1) << 8);
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
    prefetch_len_ = 0;
    prefetch_base_ = 0;
    halted_ = false;
    bus_t_ = BusT::T1;
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
    if (cs_ip == 0) { halted_ = true; return; }

    // Reset prefetch
    prefetch_base_ = cs_ip;
    prefetch_len_ = 0;

    uint8_t opbyte = fetch_byte(0);
    spdlog::info("[8088] {:04X}:{:04X} op={:02X}", regs16()[REG_CS], reg_ip_, opbyte);
    set_opcode(opbyte);
    i_w_ = (i_reg4bit_ = raw_opcode_id_ & 7) & 1;
    i_d_ = i_reg4bit_ / 2 & 1;

    // i_data0/1/2 are fetched lazily -- only when the instruction needs them.
    // The modrm block fetches what the addressing mode requires; non-modrm
    // cases fetch at point of use. This avoids 4 wasted bus reads for
    // instructions that don't need operand bytes (NOP, CLC, HLT, etc.).

    if (seg_override_en_) seg_override_en_--;
    if (rep_override_en_) rep_override_en_--;

    if (i_mod_size_) {
        i_data0_ = fetch_word(1);
        i_mod_ = (i_data0_ & 0xFF) >> 6;
        i_rm_ = i_data0_ & 7;
        i_reg_ = (i_data0_ >> 3) & 7;

        if ((!i_mod_ && i_rm_ == 6) || (i_mod_ == 2)) {
            // 16-bit displacement at [2,3]; immediate (if any) at [4,...]
            i_data1_ = fetch_word(2);
            i_imm_offset_ = 4;
        } else if (i_mod_ == 1) {
            // 8-bit displacement (sign-extended byte 2); immediate at [3,...]
            i_data1_ = (int8_t)(i_data0_ >> 8);
            i_imm_offset_ = 3;
        } else {
            // No displacement (mod=0 or mod=3); immediate at [2,...]
            i_data1_ = fetch_word(2);
            i_imm_offset_ = 2;
        }

        decode_rm_reg();
    }

    // --- Instruction execution ---
    switch (xlat_opcode_id_) {
    case 0: { // Conditional jump (Jcc)
        scratch_uchar_ = raw_opcode_id_ / 2 & 7;
        {
            uint8_t a = regs8()[TABLE[TABLE_COND_JUMP_DECODE_A][scratch_uchar_]];
            uint8_t b = regs8()[TABLE[TABLE_COND_JUMP_DECODE_B][scratch_uchar_]];
            uint8_t c = regs8()[TABLE[TABLE_COND_JUMP_DECODE_C][scratch_uchar_]];
            uint8_t d = regs8()[TABLE[TABLE_COND_JUMP_DECODE_D][scratch_uchar_]];
            int cond = i_w_ ^ (a || b || c ^ d);
            int8_t disp = (int8_t)fetch_byte(1);
            reg_ip_ += disp * cond;
        }
        break;
    }
    case 1: { // MOV reg, imm
        i_w_ = !!(raw_opcode_id_ & 8);
        uint32_t addr = get_reg_addr(i_reg4bit_);
        op_dest_ = rmem(addr);
        op_source_ = i_w_ ? fetch_word(1) : fetch_byte(1);
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
            uint32_t s = i_w_ ? fetch_word(i_imm_offset_) : fetch_byte(i_imm_offset_);
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
            bool div_err = false;
            if (i_w_) {
                scratch_int_ = (int)(unsigned short)rm;
                if (scratch_int_) {
                    scratch_uint_ = (regs16()[REG_DX] << 16) + regs16()[REG_AX];
                    scratch2_uint_ = scratch_uint_ / scratch_int_;
                    if (scratch2_uint_ == (uint16_t)scratch2_uint_) {
                        regs16()[REG_DX] = scratch_uint_ - scratch_int_ * scratch2_uint_;
                        regs16()[REG_AX] = (uint16_t)scratch2_uint_;
                    } else div_err = true;
                } else div_err = true;
            } else {
                scratch_int_ = (int)(uint8_t)rm;
                if (scratch_int_) {
                    scratch_uint_ = regs16()[REG_AX];
                    scratch2_uint_ = (uint16_t)scratch_uint_ / scratch_int_;
                    if (scratch2_uint_ == (uint8_t)scratch2_uint_) {
                        regs8()[REG_AH] = scratch_uint_ - scratch_int_ * scratch2_uint_;
                        regs8()[REG_AL] = (uint8_t)scratch2_uint_;
                    } else div_err = true;
                } else div_err = true;
            }
            if (div_err) div_error_ = true;
            break;
        }
        case 7: { // IDIV
            uint32_t rm = rmem(rm_addr_);
            bool div_err = false;
            if (i_w_) {
                scratch_int_ = (short)(uint16_t)rm;
                if (scratch_int_) {
                    scratch_uint_ = (regs16()[REG_DX] << 16) + regs16()[REG_AX];
                    scratch2_uint_ = (int)scratch_uint_ / scratch_int_;
                    if ((int)scratch2_uint_ == (short)scratch2_uint_) {
                        regs16()[REG_DX] = (int)scratch_uint_ - scratch_int_ * (int)scratch2_uint_;
                        regs16()[REG_AX] = (uint16_t)scratch2_uint_;
                    } else div_err = true;
                } else div_err = true;
            } else {
                scratch_int_ = (int8_t)(uint8_t)rm;
                if (scratch_int_) {
                    scratch_uint_ = regs16()[REG_AX];
                    scratch2_uint_ = (short)scratch_uint_ / scratch_int_;
                    if ((int)scratch2_uint_ == (int8_t)scratch2_uint_) {
                        regs8()[REG_AH] = (short)scratch_uint_ - scratch_int_ * (int)scratch2_uint_;
                        regs8()[REG_AL] = (uint8_t)scratch2_uint_;
                    } else div_err = true;
                } else div_err = true;
            }
            if (div_err) div_error_ = true;
            break;
        }
        default: break;
        }
        break;
    }
    case 7: { // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP AL/AX, imm
        rm_addr_ = REGS_BASE;
        i_mod_ = 3;
        i_reg_ = extra_;
        i_imm_offset_ = 1;
        reg_ip_--;
    }
    [[fallthrough]];
    case 8: { // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP reg, imm
        op_to_addr_ = rm_addr_;
        i_d_ |= !i_w_;
        i_data2_ = i_d_ ? fetch_byte(i_imm_offset_) : fetch_word(i_imm_offset_);
        regs16()[REG_SCRATCH] = i_d_ ? (int8_t)(i_data2_ & 0xFF) : (uint16_t)i_data2_;
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
        i_data0_ = fetch_word(1);
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
        reg_ip_ += scratch_uint_ * (int8_t)fetch_byte(1);
        break;
    }
    case 14: { // JMP | CALL short/near/far
        reg_ip_ += 3 - i_d_;
        if (i_d_ && i_w_) {
            // JMP short rel8 -- only 1 byte needed
            reg_ip_ += (int8_t)fetch_byte(1);
        } else {
            i_data0_ = fetch_word(1);
            if (!i_w_) {
                if (i_d_) { // JMP far
                    i_data2_ = fetch_word(3);
                    reg_ip_ = 0;
                    regs16()[REG_CS] = (uint16_t)i_data2_;
                } else { // CALL near
                    push16(reg_ip_);
                }
            }
            reg_ip_ += (int16_t)i_data0_;
        }
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
        else if (!i_d_) regs16()[REG_SP] += fetch_word(1); // RET/RETF imm16
        break;
    }
    case 20: { // MOV r/m, imm
        op_result_ = i_w_ ? fetch_word(i_imm_offset_) : fetch_byte(i_imm_offset_);
        wmem(op_from_addr_, op_result_);
        break;
    }
    case 21: { // IN AL/AX, DX/imm8
        scratch_uint_ = extra_ ? regs16()[REG_DX] : fetch_byte(1);
        uint8_t val = io_read_byte((uint16_t)scratch_uint_);
        regs8()[REG_AL] = val;
        if (i_w_) regs8()[REG_AH] = io_read_byte((uint16_t)(scratch_uint_ + 1));
        op_result_ = regs8()[REG_AL];
        break;
    }
    case 22: { // OUT DX/imm8, AL/AX
        scratch_uint_ = extra_ ? regs16()[REG_DX] : fetch_byte(1);
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
        i_data0_ = fetch_word(1);
        i_data2_ = fetch_word(3);
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
        pc_interrupt(fetch_byte(1));
        break;
    case 40: // INTO
        ++reg_ip_;
        if (regs8()[FLAG_OF]) pc_interrupt(4);
        break;
    case 41: { // AAM
        uint8_t divisor = fetch_byte(1);
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
        regs16()[REG_AX] = op_result_ = 0xFF & (regs8()[REG_AL] + fetch_byte(1) * regs8()[REG_AH]);
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
        uint32_t s = i_w_ ? fetch_word(1) : fetch_byte(1);
        op_dest_ = d; op_source_ = s;
        op_result_ = d & s;
        break;
    }
    case 48: // 0F xx (emulator-specific, not used on real hardware)
        break;
    case 53: // HLT
        halted_ = true;
        return;

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

    // Divide error: INT 0 fires after IP advance (8088 pushes next-instruction IP)
    if (div_error_) {
        div_error_ = false;
        pc_interrupt(0);
        return;
    }

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
        } else if (pin_intr_.level() == Level::High) {
            // INTA bus cycle: two back-to-back INTA pulses.
            // Each pulse is a full 4-T-state bus cycle, same as bus_read_byte.

            // First INTA pulse (PIC latches request) -- 4 T-states
            drive_status((BUS_INTA >> 2) & 1, (BUS_INTA >> 1) & 1, BUS_INTA & 1);
            full_wait_clk("T1 INTA Pulse 1");                    // T1
            release_data();
            full_wait_clk("T2 INTA Pulse 1");                    // T2
            drive_status_passive();
            full_wait_clk("T3 INTA Pulse 1");                    // T3
            bus_t_ = BusT::T1;
            full_wait_clk("T4 INTA Pulse 1");                    // T4

            // Second INTA pulse (PIC drives vector on data bus) -- 4 T-states
            drive_status((BUS_INTA >> 2) & 1, (BUS_INTA >> 1) & 1, BUS_INTA & 1);
            full_wait_clk("T1 INTA Pulse 2");                    // T1
            release_data();
            full_wait_clk("T2 INTA Pulse 2");                    // T2
            drive_status_passive();
            full_wait_clk("T3 INTA Pulse 2");                    // T3
            uint8_t vector = read_data();
            bus_t_ = BusT::T1;
            full_wait_clk("T4 INTA Pulse 2");                    // T4

            pc_interrupt(vector);
        }
    }
}

} // namespace bench
