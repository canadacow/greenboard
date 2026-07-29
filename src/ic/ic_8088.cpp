#include "ic/ic_8088.h"
#include "core/scheduler.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace bench {

// ========================================================================
// Constants
// ========================================================================

static constexpr int REG_AX = 0, REG_CX = 1, REG_DX = 2, REG_BX = 3;
static constexpr int REG_SP = 4, REG_BP = 5, REG_SI = 6, REG_DI = 7;
static constexpr int REG_ES = 8, REG_CS = 9, REG_SS = 10, REG_DS = 11;
static constexpr int REG_ZERO = 12, REG_SCRATCH = 13;

static constexpr int REG_AL = 0, REG_AH = 1, REG_CL = 2, REG_CH = 3;
static constexpr int REG_DL = 4, REG_DH = 5, REG_BL = 6, REG_BH = 7;

static constexpr int FLAG_CF = 40, FLAG_PF = 41, FLAG_AF = 42, FLAG_ZF = 43;
static constexpr int FLAG_SF = 44, FLAG_TF = 45, FLAG_IF = 46, FLAG_DF = 47;
static constexpr int FLAG_OF = 48;

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

// Map BusOp::Kind to bus status encoding.
static constexpr uint8_t kind_to_bus[] = {
    BUS_PASSIVE,  // NONE
    BUS_MEMR,     // MEM_READ
    BUS_MEMW,     // MEM_WRITE
    BUS_IOR,      // IO_READ
    BUS_IOW,      // IO_WRITE
    BUS_INTA,     // INTA
    BUS_FETCH,    // FETCH
    BUS_PASSIVE,  // IDLE
};

// ========================================================================
// Constructor / Destructor / Component overrides
// ========================================================================

IC_8088::IC_8088(uint16_t start_cs, uint16_t start_ip)
    : CoroComponent("8088"), start_cs_(start_cs), start_ip_(start_ip) {
    set_description("CPU");
    cpu_reset();
}

IC_8088::IC_8088(cereal::BinaryInputArchive& ar)
    : CoroComponent("8088") {
    set_description("CPU");
    serialize(ar);  // all flat state from archive, no cpu_reset()
}

IC_8088::~IC_8088() {
    power_off();
}

void IC_8088::power_on() {
    if (biu_task_.handle_) return;
    biu_task_ = biu_run();
    spdlog::debug("[8088] powered on (coroutine)");
}

void IC_8088::power_off() {
    if (!biu_task_.handle_) return;
    biu_task_.handle_.destroy();
    biu_task_.handle_ = {};
    eu_resume_ = std::noop_coroutine();
    eu_done_ = false;
    spdlog::debug("[8088] powered off (coroutine)");
}

void IC_8088::on_cycle(Fiber /*caller*/) {
    if (!biu_task_.handle_ || biu_task_.handle_.done()) return;
    biu_task_.handle_.resume();
}

// ========================================================================
// install -- pin wiring + DAG declarations (same as before)
// ========================================================================

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
    declare_bidir_block({pin_s0_, pin_s1_, pin_s2_, pin_lock_},
                        BidirDir::Output | BidirDir::HiZ,
                        [this]() {
                            return bus_t_ == BusT::T2_Read ? BidirDir::HiZ : BidirDir::Output;
                        });
}

// ========================================================================
// BIU pin operations
// ========================================================================

void IC_8088::drive_address(uint32_t address) {
    bus_t_ = BusT::T1;
    for (int i = 0; i < 8; ++i)
        pin_ad_[i].drive((address >> i) & 1 ? Level::High : Level::Low);
    for (int i = 0; i < 12; ++i)
        pin_a_upper_[i].drive((address >> (i + 8)) & 1 ? Level::High : Level::Low);
}

void IC_8088::drive_data(uint8_t value) {
    bus_t_ = BusT::T2_Write;
    for (int i = 0; i < 8; ++i)
        pin_ad_[i].drive((value >> i) & 1 ? Level::High : Level::Low);
}

uint8_t IC_8088::read_data() {
    bus_t_ = BusT::T2_Read;
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i)
        if (pin_ad_[i].level() == Level::High)
            val |= (1 << i);
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

// ========================================================================
// BIU coroutine
// ========================================================================

BIUTask IC_8088::biu_run() {
    // Wait for VCC
    while (pin_vcc_.level() != Level::High)
        co_await std::suspend_always{};

    // Wait for RESET to deassert. Register state was set by the constructor
    // (cpu_reset() for cold boot, or serialize(ar) for save-state load).
    if (pin_reset_.level() == Level::High) {
        spdlog::info("[8088] RESET asserted -- CS:IP = {:04X}:{:04X}",
                     regs16()[REG_CS], reg_ip_);
        while (pin_reset_.level() == Level::High)
            co_await std::suspend_always{};
    }

    spdlog::info("[8088] starting execution at {:04X}:{:04X}",
                 regs16()[REG_CS], reg_ip_);

    drive_status_passive();

    // Create EU coroutine
    auto eu = eu_run();
    eu_resume_ = eu.handle_;
    eu_done_ = false;

    for (;;) {
        if (pin_vcc_.level() != Level::High) break;

        // Mid-execution RESET: abort EU, reset CPU, wait for deassert, restart
        if (pin_reset_.level() == Level::High) {
            spdlog::info("[8088] RESET asserted mid-execution at instr {}", instr_count_);
            eu.handle_.destroy();
            drive_status_passive();
            release_data();
            cpu_reset();
            while (pin_reset_.level() == Level::High)
                co_await std::suspend_always{};
            spdlog::info("[8088] RESET deasserted -- restarting at {:04X}:{:04X}", start_cs_, start_ip_);
            drive_status_passive();
            eu = eu_run();
            eu_resume_ = eu.handle_;
            eu_done_ = false;
            continue;
        }

        check_nmi();
        if (breakpoint_) {
            co_await std::suspend_always{};
            continue;
        }
        // Resume EU -- runs until next bus op or completion
        eu_resume_.resume();
        if (eu_done_) break;

        // Process the bus request from the EU
        auto kind = bus_op_.kind;
        uint8_t bus_type = kind_to_bus[kind];
        uint8_t s2 = (bus_type >> 2) & 1;
        uint8_t s1 = (bus_type >> 1) & 1;
        uint8_t s0 = bus_type & 1;

        constexpr bool trace_bus = false; // Specify an address range to follow here
        auto bs = [this]() { return debug_bus_state_ ? debug_bus_state_() : std::string(""); };            

        if (kind == BusOp::MEM_READ || kind == BusOp::IO_READ || kind == BusOp::FETCH) {
            // ---- Read bus cycle: T1, T2, T3, Tw*, T4 ----

            // T1 -- drive status + address
            t_state_ = TState::T1;
            drive_status(s2, s1, s0);
            drive_address(bus_op_.addr & 0xFFFFF);
            if (trace_bus) spdlog::info("[BIU] {} | RD {:05X} T1 S={}{}{}", bs(), bus_op_.addr & 0xFFFFF, s2, s1, s0);
            check_nmi();
            co_await std::suspend_always{};

            // T2 -- ALE falls, release AD. Status stays active.
            t_state_ = TState::T2;
            release_data();
            if (trace_bus) spdlog::info("[BIU] {} | RD {:05X} T2", bs(), bus_op_.addr & 0xFFFFF);
            check_nmi();
            co_await std::suspend_always{};

            // T3 -- status goes passive. Data driven by memory/peripherals.
            t_state_ = TState::T3;
            drive_status_passive();
            if (trace_bus) spdlog::info("[BIU] {} | RD {:05X} T3", bs(), bus_op_.addr & 0xFFFFF);
            check_nmi();
            co_await std::suspend_always{};

            // Tw -- wait states while READY is low
            t_state_ = TState::Tw;
            while (pin_ready_.level() != Level::High) {
                if (trace_bus) spdlog::info("[BIU] {} | RD {:05X} Tw", bs(), bus_op_.addr & 0xFFFFF);
                check_nmi();
                co_await std::suspend_always{};
            }

            // T4 -- bus cycle complete, read data
            t_state_ = TState::T4;
            bus_op_.read_data = read_data();
            if (trace_bus) spdlog::info("[BIU] {} | RD {:05X} T4 data={:02X}", bs(), bus_op_.addr & 0xFFFFF, bus_op_.read_data);
            last_bus_tx_ = {bus_op_.addr & 0xFFFFF, bus_op_.read_data, bus_type};
            bus_t_ = BusT::T1;
            check_nmi();
            co_await std::suspend_always{};
            t_state_ = TState::Ti;

        } else if (kind == BusOp::MEM_WRITE || kind == BusOp::IO_WRITE) {
            // ---- Write bus cycle: T1, T2, T3, Tw*, T4 ----

            // T1 -- drive status + address
            t_state_ = TState::T1;
            drive_status(s2, s1, s0);
            drive_address(bus_op_.addr & 0xFFFFF);
            if (trace_bus) spdlog::info("[BIU] {} | WR {:05X}={:02X} T1 S={}{}{}", bs(), bus_op_.addr & 0xFFFFF, bus_op_.write_data, s2, s1, s0);
            check_nmi();
            co_await std::suspend_always{};

            // T2 -- ALE falls. Switch AD to write data. Status stays active.
            t_state_ = TState::T2;
            drive_data(bus_op_.write_data);
            if (trace_bus) spdlog::info("[BIU] {} | WR {:05X}={:02X} T2", bs(), bus_op_.addr & 0xFFFFF, bus_op_.write_data);
            check_nmi();
            co_await std::suspend_always{};

            // T3 -- status goes passive. Data held on bus.
            t_state_ = TState::T3;
            drive_status_passive();
            if (trace_bus) spdlog::info("[BIU] {} | WR {:05X}={:02X} T3", bs(), bus_op_.addr & 0xFFFFF, bus_op_.write_data);
            check_nmi();
            co_await std::suspend_always{};

            // Tw -- wait states while READY is low
            t_state_ = TState::Tw;
            while (pin_ready_.level() != Level::High) {
                if (trace_bus) spdlog::info("[BIU] {} | WR {:05X}={:02X} Tw", bs(), bus_op_.addr & 0xFFFFF, bus_op_.write_data);
                check_nmi();
                co_await std::suspend_always{};
            }

            // T4 -- bus cycle complete, release data bus
            t_state_ = TState::T4;
            if (trace_bus) spdlog::info("[BIU] {} | WR {:05X}={:02X} T4", bs(), bus_op_.addr & 0xFFFFF, bus_op_.write_data);
            release_data();
            last_bus_tx_ = {bus_op_.addr & 0xFFFFF, bus_op_.write_data, bus_type};
            bus_t_ = BusT::T1;
            check_nmi();
            co_await std::suspend_always{};
            t_state_ = TState::Ti;

        } else if (kind == BusOp::INTA) {
            // ---- INTA: two back-to-back bus cycles ----

            // First INTA pulse (PIC latches request) -- T1,T2,T3,Tw*,T4
            t_state_ = TState::T1;
            drive_status(s2, s1, s0);
            check_nmi();
            co_await std::suspend_always{};

            t_state_ = TState::T2;
            release_data();
            check_nmi();
            co_await std::suspend_always{};

            t_state_ = TState::T3;
            drive_status_passive();
            check_nmi();
            co_await std::suspend_always{};

            t_state_ = TState::Tw;
            while (pin_ready_.level() != Level::High) {
                check_nmi();
                co_await std::suspend_always{};
            }

            t_state_ = TState::T4;
            bus_t_ = BusT::T1;
            check_nmi();
            co_await std::suspend_always{};

            // Second INTA pulse (PIC drives vector on data bus) -- T1,T2,T3,Tw*,T4
            t_state_ = TState::T1;
            drive_status(s2, s1, s0);
            check_nmi();
            co_await std::suspend_always{};

            t_state_ = TState::T2;
            release_data();
            check_nmi();
            co_await std::suspend_always{};

            t_state_ = TState::T3;
            drive_status_passive();
            check_nmi();
            co_await std::suspend_always{};

            t_state_ = TState::Tw;
            while (pin_ready_.level() != Level::High) {
                check_nmi();
                co_await std::suspend_always{};
            }

            t_state_ = TState::T4;
            bus_op_.read_data = read_data();
            bus_t_ = BusT::T1;
            check_nmi();
            co_await std::suspend_always{};
            t_state_ = TState::Ti;

        } else if (kind == BusOp::IDLE) {
            // ---- EU-internal execution time: bus stays passive ----
            t_state_ = TState::Ti;
            for (uint32_t n = bus_op_.addr; n; --n) {
                check_nmi();
                co_await std::suspend_always{};
            }
        }
    }

    // Cleanup pins
    drive_status_passive();
    release_data();
    for (int i = 0; i < 12; ++i)
        pin_a_upper_[i].release();
    pin_qs0_.release();
    pin_qs1_.release();

    eu.handle_.destroy();
}

// ========================================================================
// Pure ALU functions
// ========================================================================

void IC_8088::check_nmi() {
    Level cur = pin_nmi_.level();
    if (cur == Level::High && nmi_prev_ != Level::High)
        nmi_pending_ = true;
    nmi_prev_ = cur;
}

void IC_8088::cpu_reset() {
    std::memset(regs_, 0, sizeof(regs_));
    regs16()[REG_CS] = start_cs_;
    reg_ip_ = start_ip_;
    seg_override_en_ = 0;
    rep_override_en_ = 0;
    trap_flag_ = 0;
    nmi_pending_ = false;
    prefetch_base_ = 0;
    halted_ = false;
    breakpoint_ = false;
    bus_t_ = BusT::T1;
    t_state_ = TState::Ti;
    instr_count_ = 0;
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

int IC_8088::AAA_AAS(int which_operation) {
    int adj = ((regs8()[REG_AL] & 0x0F) > 9) || regs8()[FLAG_AF];
    set_AF(adj); set_CF(adj);
    if (adj) regs16()[REG_AX] += 262 * which_operation;
    regs8()[REG_AL] &= 0x0F;
    return regs8()[REG_AL];
}

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
// EU main loop (single coroutine frame -- execute() inlined)
// ========================================================================

EUTask<void> IC_8088::eu_run() {
    for (;;) {
    // --- begin inlined execute() ---
    uint32_t cs_ip = 16u * regs16()[REG_CS] + reg_ip_;
    bool advanceIp = true;

    if (cs_ip == 0) { halted_ = true; break; }

#if BENCH_CFG_TRACE
    if (tracer_)
        tracer_->on_instruction(cs_ip, regs16()[REG_CS], reg_ip_, instr_count_);
#endif

    prefetch_base_ = cs_ip;

    uint8_t opbyte = co_await fetch_byte(0);
    set_opcode(opbyte);
    i_w_ = (i_reg4bit_ = raw_opcode_id_ & 7) & 1;
    i_d_ = i_reg4bit_ / 2 & 1;

    if (seg_override_en_) seg_override_en_--;
    if (rep_override_en_) rep_override_en_--;

    if (i_mod_size_) {
        // Fetch exactly the bytes the instruction encodes. The 8086tiny
        // ancestor speculatively read words at offsets 1 and 2 for every
        // ModRM instruction (free on its flat memory); on a real bus that
        // taxed register-form instructions with up to 3 phantom fetches
        // (12 CLK) each.
        i_data0_ = co_await fetch_byte(1);   // ModRM byte
        i_mod_ = (i_data0_ & 0xFF) >> 6;
        i_rm_ = i_data0_ & 7;
        i_reg_ = (i_data0_ >> 3) & 7;

        if ((!i_mod_ && i_rm_ == 6) || (i_mod_ == 2)) {
            FETCH_WORD_(2, i_data1_);        // disp16
            i_imm_offset_ = 4;
        } else if (i_mod_ == 1) {
            i_data1_ = (int8_t)(co_await fetch_byte(2));  // disp8
            i_imm_offset_ = 3;
        } else {
            i_imm_offset_ = 2;               // no displacement
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
            int8_t disp = (int8_t) co_await fetch_byte(1);
            reg_ip_ += disp * cond;
        }
        break;
    }
    case 1: { // MOV reg, imm
        i_w_ = !!(raw_opcode_id_ & 8);
        uint32_t addr = get_reg_addr(i_reg4bit_);
        RMEM_(addr, op_dest_);
        if (i_w_) { FETCH_WORD_(1, op_source_); }
        else { op_source_ = co_await fetch_byte(1); }
        op_result_ = op_source_;
        WMEM_(addr, op_result_);
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
            RMEM_(op_from_addr_, op_dest_);
            op_source_ = 1;
            op_result_ = op_dest_ + 1 - 2 * i_reg_;
            WMEM_(op_from_addr_, op_result_);
            set_AF_OF_arith();
            set_OF(op_dest_ + 1 - i_reg_ == (uint32_t)(1 << (top_bit() - 1)));
            if (xlat_opcode_id_ == 5) set_opcode(0x10);
        } else if (i_reg_ != 6) {
            if (i_reg_ == 3) { PUSH16_(regs16()[REG_CS]); }
            if (i_reg_ & 2) { PUSH16_(reg_ip_ + 2 + i_mod_ * (i_mod_ != 3) + 2 * (!i_mod_ && i_rm_ == 6)); }
            if (i_reg_ & 1) { RMEM16_(op_from_addr_ + 2, scratch_uint_); regs16()[REG_CS] = (uint16_t)scratch_uint_; }
            { RMEM16_(op_from_addr_, scratch_uint_); reg_ip_ = (uint16_t)scratch_uint_; }
            op_result_ = reg_ip_;
            set_opcode(0x9A);
        } else {
            i_w_ = 1;
            uint32_t _tmp; RMEM16_(rm_addr_, _tmp);
            PUSH16_((uint16_t)_tmp);
        }
        break;
    }
    case 6: { // TEST r/m,imm / NOT|NEG|MUL|IMUL|DIV|IDIV
        op_to_addr_ = op_from_addr_;
        switch (i_reg_) {
        case 0: { // TEST
            set_opcode(0x20);
            reg_ip_ += i_w_ + 1;
            RMEM_(op_to_addr_, op_dest_);
            if (i_w_) { FETCH_WORD_(i_imm_offset_, op_source_); }
            else { op_source_ = co_await fetch_byte(i_imm_offset_); }
            op_result_ = i_w_ ? (uint16_t)(op_dest_ & op_source_) : (uint8_t)(op_dest_ & op_source_);
            break;
        }
        case 2: { // NOT
            RMEM_(op_to_addr_, op_dest_);
            op_result_ = ~op_dest_;
            WMEM_(op_to_addr_, op_result_);
            break;
        }
        case 3: { // NEG
            uint32_t d; RMEM_(op_to_addr_, d);
            op_dest_ = 0;
            op_source_ = d;
            op_result_ = -(int)d;
            WMEM_(op_to_addr_, op_result_);
            set_opcode(0x28);
            set_CF(op_result_ > op_dest_);
            break;
        }
        case 4: { // MUL
            uint32_t rm; RMEM_(rm_addr_, rm);
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
            uint32_t rm; RMEM_(rm_addr_, rm);
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
            uint32_t rm; RMEM_(rm_addr_, rm);
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
            uint32_t rm; RMEM_(rm_addr_, rm);
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
        if (i_d_) { i_data2_ = co_await fetch_byte(i_imm_offset_); }
        else { FETCH_WORD_(i_imm_offset_, i_data2_); }
        regs16()[REG_SCRATCH] = i_d_ ? (int8_t)(i_data2_ & 0xFF) : (uint16_t)i_data2_;
        op_from_addr_ = REGS_BASE + 2 * REG_SCRATCH;
        reg_ip_ += !i_d_ + 1;
        set_opcode(0x08 * (extra_ = i_reg_));
    }
    [[fallthrough]];
    case 9: { // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP|MOV reg, r/m
        uint32_t d, s;
        RMEM_(op_to_addr_, d);
        RMEM_(op_from_addr_, s);
        op_dest_ = d; op_source_ = s;
        switch (extra_) {
        case 0: // ADD
            op_result_ = d + s;
            WMEM_(op_to_addr_, op_result_);
            set_CF(i_w_ ? (uint16_t)op_result_ < (uint16_t)d : (uint8_t)op_result_ < (uint8_t)d);
            break;
        case 1: // OR
            op_result_ = d | s;
            WMEM_(op_to_addr_, op_result_);
            break;
        case 2: { // ADC
            int cf = regs8()[FLAG_CF];
            op_result_ = d + s + cf;
            WMEM_(op_to_addr_, op_result_);
            set_CF((cf && (uint32_t)op_result_ == d) ||
                   (i_w_ ? (uint16_t)op_result_ < (uint16_t)d : (uint8_t)op_result_ < (uint8_t)d));
            set_AF_OF_arith();
            break;
        }
        case 3: { // SBB
            int cf = regs8()[FLAG_CF];
            op_result_ = d - s - cf;
            WMEM_(op_to_addr_, op_result_);
            set_CF((cf && (uint32_t)op_result_ == d) ||
                   (i_w_ ? (uint16_t)d < (uint16_t)(s + cf) : (uint8_t)d < (uint8_t)(s + cf)));
            set_AF_OF_arith();
            break;
        }
        case 4: // AND
            op_result_ = d & s;
            WMEM_(op_to_addr_, op_result_);
            break;
        case 5: // SUB
            op_result_ = d - s;
            WMEM_(op_to_addr_, op_result_);
            set_CF(i_w_ ? (uint16_t)d < (uint16_t)s : (uint8_t)d < (uint8_t)s);
            break;
        case 6: // XOR
            op_result_ = d ^ s;
            WMEM_(op_to_addr_, op_result_);
            break;
        case 7: // CMP
            op_result_ = d - s;
            set_CF(i_w_ ? (uint16_t)d < (uint16_t)s : (uint8_t)d < (uint8_t)s);
            break;
        case 8: // MOV
            op_result_ = s;
            WMEM_(op_to_addr_, op_result_);
            break;
        }
        break;
    }
    case 10: { // MOV sreg,r/m | POP r/m | LEA reg,r/m
        if (!i_w_) {
            i_w_ = 1;
            i_reg_ += 8;
            decode_rm_reg();
            RMEM_(op_from_addr_, op_result_);
            WMEM_(op_to_addr_, op_result_);
        } else if (!i_d_) {
            seg_override_en_ = 1;
            seg_override_ = REG_ZERO;
            decode_rm_reg();
            WMEM16_(op_from_addr_, rm_addr_);
            op_result_ = rm_addr_;
        } else {
            i_w_ = 1;
            uint16_t val; POP16_(val);
            WMEM16_(rm_addr_, val);
            op_result_ = val;
        }
        break;
    }
    case 11: { // MOV AL/AX, [loc]
        FETCH_WORD_(1, i_data0_);
        i_mod_ = 0; i_reg_ = 0; i_rm_ = 6;
        i_data1_ = i_data0_;
        decode_rm_reg();
        RMEM_(op_to_addr_, op_result_);
        WMEM_(op_from_addr_, op_result_);
        break;
    }
    case 12: { // ROL|ROR|RCL|RCR|SHL|SHR|???|SAR reg/mem, 1/CL/imm
        uint32_t val; RMEM_(rm_addr_, val);
        scratch2_uint_ = sign_of(val);
        if (extra_) {
            // imm8 count form: fetch the count byte after ModRM+disp.
            // (Previously read from i_data1_, which only held this byte by
            // accident of the speculative decode fetch -- and held the
            // displacement instead for mod==1 forms.)
            scratch_uint_ = (int8_t) co_await fetch_byte(i_imm_offset_);
            ++reg_ip_;
        } else {
            scratch_uint_ = i_d_ ? regs8()[REG_CL]  // 8088: no 5-bit mask (186+ masks to 31)
                                 : 1;
        }
        if (scratch_uint_) {
            if (i_reg_ < 4) {
                scratch_uint_ %= i_reg_ / 2 + top_bit();
                RMEM_(rm_addr_, scratch2_uint_);
            }
            if (i_reg_ & 1) {
                RMEM_(rm_addr_, op_dest_); op_source_ = scratch_uint_;
                op_result_ = i_w_ ? (uint16_t)op_dest_ >> scratch_uint_ : (uint8_t)op_dest_ >> scratch_uint_;
                WMEM_(rm_addr_, op_result_);
            } else {
                RMEM_(rm_addr_, op_dest_); op_source_ = scratch_uint_;
                op_result_ = op_dest_ << scratch_uint_;
                WMEM_(rm_addr_, op_result_);
            }
            if (i_reg_ > 3) set_opcode(0x10);
            if (i_reg_ > 4) set_CF(op_dest_ >> (scratch_uint_ - 1) & 1);
        }
        switch (i_reg_) {
        case 0: { // ROL
            uint32_t combined; RMEM_(rm_addr_, combined);
            combined += scratch2_uint_ >> (top_bit() - scratch_uint_);
            op_result_ = combined;
            WMEM_(rm_addr_, op_result_);
            set_OF(sign_of(op_result_) ^ set_CF(op_result_ & 1));
            break;
        }
        case 1: { // ROR
            scratch2_uint_ &= (1 << scratch_uint_) - 1;
            uint32_t combined; RMEM_(rm_addr_, combined);
            combined += scratch2_uint_ << (top_bit() - scratch_uint_);
            op_result_ = combined;
            WMEM_(rm_addr_, op_result_);
            set_OF(sign_of(op_result_ * 2) ^ set_CF(sign_of(op_result_)));
            break;
        }
        case 2: { // RCL
            uint32_t combined; RMEM_(rm_addr_, combined);
            combined += (regs8()[FLAG_CF] << (scratch_uint_ - 1))
                      + (scratch2_uint_ >> (1 + top_bit() - scratch_uint_));
            op_result_ = combined;
            WMEM_(rm_addr_, op_result_);
            set_OF(sign_of(op_result_) ^ set_CF(scratch2_uint_ & (1 << (top_bit() - scratch_uint_))));
            break;
        }
        case 3: { // RCR
            uint32_t combined; RMEM_(rm_addr_, combined);
            combined += (regs8()[FLAG_CF] << (top_bit() - scratch_uint_))
                      + (scratch2_uint_ << (1 + top_bit() - scratch_uint_));
            op_result_ = combined;
            WMEM_(rm_addr_, op_result_);
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
            uint32_t fill = scratch2_uint_ * ~(((1u << top_bit()) - 1) >> scratch_uint_);
            uint32_t combined; RMEM_(rm_addr_, combined);
            combined += fill;
            op_result_ = combined;
            WMEM_(rm_addr_, op_result_);
            break;
        }
        default: break;
        }
        break;
    }
    case 13: { // LOOPxx|JCXZ
        scratch_uint_ = !!--regs16()[REG_CX];
        switch (i_reg4bit_) {
        case 0: scratch_uint_ &= !regs8()[FLAG_ZF]; break;
        case 1: scratch_uint_ &= regs8()[FLAG_ZF]; break;
        case 3: scratch_uint_ = !++regs16()[REG_CX]; break;
        }
        reg_ip_ += scratch_uint_ * (int8_t) co_await fetch_byte(1);
        break;
    }
    case 14: { // JMP | CALL short/near/far
        reg_ip_ += 3 - i_d_;
        if (i_d_ && i_w_) {
            reg_ip_ += (int8_t) co_await fetch_byte(1);
        } else {
            FETCH_WORD_(1, i_data0_);
            if (!i_w_) {
                if (i_d_) {
                    FETCH_WORD_(3, i_data2_);
                    reg_ip_ = 0;
                    regs16()[REG_CS] = (uint16_t)i_data2_;
                } else {
                    PUSH16_(reg_ip_);
                }
            }
            reg_ip_ += (int16_t)i_data0_;
        }
        break;
    }
    case 15: { // TEST reg, r/m
        uint32_t d, s;
        RMEM_(op_from_addr_, d);
        RMEM_(op_to_addr_, s);
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
            uint32_t a, b;
            RMEM_(op_to_addr_, a);
            RMEM_(op_from_addr_, b);
            WMEM_(op_to_addr_, b);
            WMEM_(op_from_addr_, a);
            op_result_ = b;
        }
        break;
    }
    case 17: { // MOVSx|STOSx|LODSx
        scratch2_uint_ = seg_override_en_ ? seg_override_ : REG_DS;
        // Real 8088 REP cost per iteration (total incl. bus): MOVS 17,
        // STOS 10, LODS 13. The bus transfers cost 8/4/4 CLK, so pad the
        // remainder as EU-internal idle time.
        for (scratch_uint_ = rep_override_en_ ? regs16()[REG_CX] : 1;
             scratch_uint_; ) {
            uint32_t src_addr = (extra_ & 1)
                ? REGS_BASE
                : 16u * regs16()[scratch2_uint_] + regs16()[REG_SI];
            uint32_t dst_addr = (extra_ < 2)
                ? 16u * regs16()[REG_ES] + regs16()[REG_DI]
                : REGS_BASE;
            uint32_t val; RMEM_(src_addr, val);
            op_result_ = val;
            WMEM_(dst_addr, val);
            if (!(extra_ & 1)) index_inc(REG_SI);
            if (!(extra_ & 2)) index_inc(REG_DI);
            scratch_uint_--;
            if (rep_override_en_) {
                co_await eu_idle(extra_ == 1 ? 6 : 9);
                regs16()[REG_CX] = (uint16_t)scratch_uint_;
                // Real 8088: REP is interruptible between iterations. On a
                // pending interrupt, resume at the immediately preceding
                // prefix after the ISR (faithful to the 8088 quirk of
                // dropping all but the last prefix). Condition must mirror
                // the dispatch gate at instruction end or we rewind forever.
                if (scratch_uint_ &&
                    regs8()[FLAG_IF] && pin_intr_.level() == Level::High) {
                    reg_ip_ -= 1;
                    advanceIp = false;
                    rep_override_en_ = 0;  // permit dispatch at instruction end
                    break;
                }
            }
        }
        break;
    }
    case 18: { // CMPSx|SCASx
        scratch2_uint_ = seg_override_en_ ? seg_override_ : REG_DS;
        scratch_uint_ = rep_override_en_ ? regs16()[REG_CX] : 1;
        if (scratch_uint_) {
            for (; scratch_uint_; rep_override_en_ || scratch_uint_--) {
                uint32_t src_addr = extra_
                    ? REGS_BASE
                    : 16u * regs16()[scratch2_uint_] + regs16()[REG_SI];
                uint32_t cmp_addr = 16u * regs16()[REG_ES] + regs16()[REG_DI];
                uint32_t d, s;
                RMEM_(src_addr, d);
                RMEM_(cmp_addr, s);
                op_dest_ = d; op_source_ = s;
                op_result_ = d - s;
                if (!extra_) index_inc(REG_SI);
                index_inc(REG_DI);
                if (rep_override_en_ && !(--regs16()[REG_CX] && (!op_result_ == rep_mode_)))
                    scratch_uint_ = 0;
                if (rep_override_en_) {
                    // Real 8088 REP cost per iteration: CMPS 22, SCAS 15
                    // (bus transfers are 8/4 CLK of that).
                    co_await eu_idle(extra_ ? 11 : 14);
                    // Interruptible between iterations -- see case 17.
                    if (scratch_uint_ &&
                        regs8()[FLAG_IF] && pin_intr_.level() == Level::High) {
                        reg_ip_ -= 1;
                        advanceIp = false;
                        rep_override_en_ = 0;
                        break;
                    }
                }
            }
            set_flags_type_ = FLAGS_UPDATE_SZP | FLAGS_UPDATE_AO_ARITH;
            set_CF(i_w_ ? (uint16_t)op_dest_ < (uint16_t)op_source_
                        : (uint8_t)op_dest_ < (uint8_t)op_source_);
        }
        break;
    }
    case 19: { // RET|RETF|IRET
        i_d_ = i_w_;
        POP16_(reg_ip_);
        if (extra_) { uint16_t _cs; POP16_(_cs); regs16()[REG_CS] = _cs; }
        if (extra_ & 2) {
            uint16_t _fl; POP16_(_fl); set_flags(_fl);
        }
        else if (!i_d_) { uint16_t _sp; FETCH_WORD_(1, _sp); regs16()[REG_SP] += _sp; }
        /* Log return from INT 13h — works for IRET, RETF, and RETF n */
        if (int13_pending_ && extra_ &&
            regs16()[REG_CS] == int13_ret_cs_ &&
            reg_ip_ == int13_ret_ip_) {
            int13_pending_ = false;
            spdlog::info("[8088] INT 13h returned: AH={:02X} CF={} AL={:02X} "
                         "CX={:04X} DX={:04X} ES:BX={:04X}:{:04X} -> {:04X}:{:04X}",
                         regs8()[REG_AH], regs8()[FLAG_CF] ? 1 : 0,
                         regs8()[REG_AL],
                         regs16()[REG_CX], regs16()[REG_DX],
                         regs16()[REG_ES], regs16()[REG_BX],
                         regs16()[REG_CS], reg_ip_);
        }
        break;
    }
    case 20: { // MOV r/m, imm
        if (i_w_) { FETCH_WORD_(i_imm_offset_, op_result_); }
        else { op_result_ = co_await fetch_byte(i_imm_offset_); }
        WMEM_(op_from_addr_, op_result_);
        break;
    }
    case 21: { // IN AL/AX, DX/imm8
        scratch_uint_ = extra_ ? regs16()[REG_DX] : co_await fetch_byte(1);
        uint8_t val = co_await eu_io_read_byte((uint16_t)scratch_uint_);
        regs8()[REG_AL] = val;
#if BENCH_CFG_TRACE
        // Port reads are machine INPUTS -- nothing else in the trace predicts
        // them, so offline replay needs them recorded verbatim.
        if (tracer_)
            tracer_->on_port_read((uint16_t)scratch_uint_, val,
                                  regs16()[REG_CS], reg_ip_, instr_count_);
#endif
        if (i_w_) {
            regs8()[REG_AH] = co_await eu_io_read_byte((uint16_t)(scratch_uint_ + 1));
#if BENCH_CFG_TRACE
            if (tracer_)
                tracer_->on_port_read((uint16_t)(scratch_uint_ + 1), regs8()[REG_AH],
                                      regs16()[REG_CS], reg_ip_, instr_count_);
#endif
        }
        op_result_ = regs8()[REG_AL];
        break;
    }
    case 22: { // OUT DX/imm8, AL/AX
        scratch_uint_ = extra_ ? regs16()[REG_DX] : co_await fetch_byte(1);
        co_await eu_io_write_byte((uint16_t)scratch_uint_, regs8()[REG_AL]);
#if BENCH_CFG_TRACE
        // Port writes are derivable by replaying the CFG, but recording them
        // makes the trace directly interpretable -- e.g. the video mode
        // register (3D8/3B8) that says how to read the framebuffer bytes.
        if (tracer_)
            tracer_->on_port_write((uint16_t)scratch_uint_, regs8()[REG_AL],
                                   regs16()[REG_CS], reg_ip_, instr_count_);
#endif
        if (i_w_) {
            co_await eu_io_write_byte((uint16_t)(scratch_uint_ + 1), regs8()[REG_AH]);
#if BENCH_CFG_TRACE
            if (tracer_)
                tracer_->on_port_write((uint16_t)(scratch_uint_ + 1), regs8()[REG_AH],
                                       regs16()[REG_CS], reg_ip_, instr_count_);
#endif
        }
        break;
    }
    case 23: { // REPxx
        rep_override_en_ = 2;
        rep_mode_ = i_w_;
        if (seg_override_en_) seg_override_en_++;
        break;
    }
    case 25: { // PUSH reg
        PUSH16_(regs16()[extra_]);
        break;
    }
    case 26: { // POP reg
        POP16_(regs16()[extra_]);
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
    case 32: { // CALL FAR imm16:imm16
        FETCH_WORD_(1, i_data0_);
        FETCH_WORD_(3, i_data2_);
        PUSH16_(regs16()[REG_CS]);
        PUSH16_(reg_ip_ + 5);
        regs16()[REG_CS] = (uint16_t)i_data2_;
        reg_ip_ = (uint16_t)i_data0_;
        break;
    }
    case 33: // PUSHF
        make_flags();
        PUSH16_((uint16_t)scratch_uint_);
        break;
    case 34: { // POPF
        uint16_t _fl; POP16_(_fl);
        set_flags(_fl);
        break;
    }
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
        uint32_t val; RMEM16_(op_from_addr_, val);
        WMEM16_(op_to_addr_, (uint16_t)val);
        uint32_t seg_val; RMEM16_(rm_addr_ + 2, seg_val);
        WMEM16_(REGS_BASE + extra_, (uint16_t)seg_val);
        op_result_ = val;
        break;
    }
    case 38: { // INT 3 (breakpoint)
        ++reg_ip_;
        // Check IVT vector 3 (address 0x000C). If null, this is our test
        // breakpoint hook. If populated, treat as a real software interrupt.
        {
            uint8_t v0 = co_await eu_bus_read_byte(0x0C);
            uint8_t v1 = co_await eu_bus_read_byte(0x0D);
            uint8_t v2 = co_await eu_bus_read_byte(0x0E);
            uint8_t v3 = co_await eu_bus_read_byte(0x0F);
            if (v0 == 0 && v1 == 0 && v2 == 0 && v3 == 0)
                breakpoint_ = true;
        }
        PC_INTERRUPT_(3);
        break;
    }
    case 39: { // INT imm8
        reg_ip_ += 2;
        uint8_t _intnum = co_await fetch_byte(1);
        if (_intnum == 0x13 || _intnum == 0x19) {
            spdlog::info("[8088] INT 0x{:02X}: AH={:02X} AL={:02X} CX={:04X} DX={:04X} "
                         "BX={:04X} ES={:04X} CS:IP={:04X}:{:04X}",
                         _intnum, regs8()[REG_AH], regs8()[REG_AL],
                         regs16()[REG_CX], regs16()[REG_DX],
                         regs16()[REG_BX], regs16()[REG_ES],
                         regs16()[REG_CS], reg_ip_ - 2);
            if (_intnum == 0x13 && !int13_pending_) {
                /* Save caller's CS:IP BEFORE PC_INTERRUPT_ pushes.
                   Only save for outermost INT 13h (not nested). */
                int13_pending_ = true;
                int13_ret_cs_ = regs16()[REG_CS];
                int13_ret_ip_ = reg_ip_;  /* already advanced past INT xx */
            }
        }
        PC_INTERRUPT_(_intnum);
        break;
    }
    case 40: // INTO
        ++reg_ip_;
        if (regs8()[FLAG_OF]) { PC_INTERRUPT_(4); }
        break;
    case 41: { // AAM
        uint8_t divisor = co_await fetch_byte(1);
        if (divisor) {
            regs8()[REG_AH] = regs8()[REG_AL] / divisor;
            op_result_ = regs8()[REG_AL] %= divisor;
        } else {
            PC_INTERRUPT_(0);
        }
        break;
    }
    case 42: { // AAD
        i_w_ = 0;
        uint8_t _base = co_await fetch_byte(1);
        regs16()[REG_AX] = op_result_ = 0xFF & (regs8()[REG_AL] + _base * regs8()[REG_AH]);
        break;
    }
    case 43: // SALC
        regs8()[REG_AL] = -regs8()[FLAG_CF];
        break;
    case 44: { // XLAT
        uint32_t seg = seg_override_en_ ? seg_override_ : REG_DS;
        regs8()[REG_AL] = co_await rmem8(16u * regs16()[seg] + (uint16_t)(regs16()[REG_BX] + regs8()[REG_AL]));
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
        uint32_t s;
        if (i_w_) { FETCH_WORD_(1, s); }
        else { s = co_await fetch_byte(1); }
        op_dest_ = d; op_source_ = s;
        op_result_ = d & s;
        break;
    }
    case 48: // 0F xx (emulator-specific, not used on real hardware)
        break;
    case 53: { // HLT
        bool irq = (regs8()[FLAG_IF] && pin_intr_.level() == Level::High) || nmi_pending_;
        advanceIp = irq;
        break;
        }
    case 54: // ESC (D8-DF): coprocessor opcode, no-op without 8087
        break;

    case 3: // PUSH regs16
        // 8088 PUSH SP bug: pushes SP-2 (the already-decremented value),
        // not the original SP.  Detection software (e.g. Cosmo, CheckIt)
        // relies on this to distinguish 8086/88 from 286+.
        if (i_reg4bit_ == REG_SP) {
            regs16()[REG_SP] -= 2;
            uint32_t _pa = 16u * regs16()[REG_SS] + regs16()[REG_SP];
            uint16_t _pv = regs16()[REG_SP];  // already decremented
            co_await eu_bus_write_byte(_pa, _pv & 0xFF);
            co_await eu_bus_write_byte(_pa + 1, (_pv >> 8) & 0xFF);
        } else {
            PUSH16_(regs16()[i_reg4bit_]);
        }
        break;
    case 4: // POP regs16
        POP16_(regs16()[i_reg4bit_]);
        break;
    default:
        break;
    }

    // Advance IP (HLT: don't advance while halted -- reentrant instruction)
    if (advanceIp)
        reg_ip_ += (i_mod_ * (i_mod_ != 3) + 2 * (!i_mod_ && i_rm_ == 6)) * i_mod_size_
                 + TABLE[TABLE_BASE_INST_SIZE][raw_opcode_id_]
                 + TABLE[TABLE_I_W_SIZE][raw_opcode_id_] * (i_w_ + 1);

    // Divide error
    if (div_error_) {
        div_error_ = false;
        PC_INTERRUPT_(0);
        ++instr_count_;
        continue;
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
    if (trap_flag_) { PC_INTERRUPT_(1); }
    trap_flag_ = regs8()[FLAG_TF];

    // Interrupt check (not taken during HLT -- handled after HaltAwaiter)
    if (regs8()[FLAG_IF] && !seg_override_en_ && !rep_override_en_ && !regs8()[FLAG_TF]) {
        if (nmi_pending_) {
            nmi_pending_ = false;
            PC_INTERRUPT_(2);
        } else if (pin_intr_.level() == Level::High) {
            uint8_t _vec = co_await IntaAwaiter{*this};
            PC_INTERRUPT_(_vec);
            if (_vec == 0x0E) {
                spdlog::info("[8088] INT 0Eh dispatched, ISR at {:04X}:{:04X}",
                             regs16()[REG_CS], reg_ip_);
            }
        }
    }

    ++instr_count_;

    } // for (;;)
    eu_done_ = true;
}

} // namespace bench
