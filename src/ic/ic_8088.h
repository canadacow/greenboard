#pragma once
// IC_8088: Intel 8088 CPU (maximum mode) with separate BIU/EU C++20 coroutines.
//
// 40-pin DIP. U3 on the 5150 motherboard.
//
// The BIU (Bus Interface Unit) coroutine owns all external pins and runs
// the T-state bus protocol, yielding to the harness at each T-state.
//
// The EU (Execution Unit) coroutine does decode/ALU/flags and co_awaits
// bus operations which suspend it back to the BIU.
//
// All bus operations are either zero-frame awaiters or macros that expand
// inline. execute() is inlined into eu_run() -- single coroutine frame.

#include "core/coro_component.h"
#include "board/socket.h"
#include <coroutine>
#include <cstdint>
#include <cstring>

#if BENCH_CFG_TRACE
#include "debug/cfg_tracer.h"
#endif

namespace bench {

class Scheduler;

// ========================================================================
// Coroutine types
// ========================================================================

// EU task with symmetric transfer. Supports value return and void.
template <typename T>
struct EUTask {
    struct promise_type {
        T result_{};
        std::coroutine_handle<> continuation_{std::noop_coroutine()};

        EUTask get_return_object() {
            return EUTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            struct Awaiter {
                std::coroutine_handle<> cont;
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept { return cont; }
                void await_resume() noexcept {}
            };
            return Awaiter{continuation_};
        }
        void return_value(T v) { result_ = v; }
        void unhandled_exception() {}
    };
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type handle_{};

    bool await_ready() noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
        handle_.promise().continuation_ = caller;
        return handle_;
    }
    T await_resume() {
        T r = handle_.promise().result_;
        handle_.destroy();
        return r;
    }
};

template <>
struct EUTask<void> {
    struct promise_type {
        std::coroutine_handle<> continuation_{std::noop_coroutine()};

        EUTask get_return_object() {
            return EUTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            struct Awaiter {
                std::coroutine_handle<> cont;
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept { return cont; }
                void await_resume() noexcept {}
            };
            return Awaiter{continuation_};
        }
        void return_void() {}
        void unhandled_exception() {}
    };
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type handle_{};

    bool await_ready() noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
        handle_.promise().continuation_ = caller;
        return handle_;
    }
    void await_resume() { handle_.destroy(); }
};

// BIU top-level coroutine. The harness calls handle.resume() at each T-state.
struct BIUTask {
    struct promise_type {
        BIUTask get_return_object() {
            return BIUTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type handle_{};
};

// Bus operation descriptor shared between EU and BIU.
struct BusOp {
    enum Kind : uint8_t { NONE, MEM_READ, MEM_WRITE, IO_READ, IO_WRITE, INTA, FETCH, IDLE };
    Kind kind = NONE;
    uint32_t addr = 0;
    uint8_t write_data = 0;
    uint8_t read_data = 0;
};

// ========================================================================
// IC_8088
// ========================================================================

class IC_8088 : public CoroComponent {
public:
    // Bus cycle phase (actual T-state visible to debugger)
    enum class TState : uint8_t { Ti, T1, T2, T3, Tw, T4 };
    // Bidir direction hint (for DAG permutation selection)
    enum class BusT : uint8_t { T1, T2_Read, T2_Write };

    IC_8088(uint16_t start_cs = 0xF000, uint16_t start_ip = 0x0100);
    explicit IC_8088(cereal::BinaryInputArchive& ar);  // construct from save-state
    ~IC_8088() override;

    void install(Socket& socket);
    void set_scheduler(Scheduler* s) { scheduler_ = s; }

    // Component overrides
    void power_on() override;
    void power_off() override;
    bool is_powered() const override { return biu_task_.handle_ != nullptr; }

    // ====================================================================
    // Zero-frame awaiters (live on caller's coroutine frame)
    // ====================================================================

    // Bus read awaiter: sets bus_op_, suspends to BIU, returns read_data.
    struct BusReadAwaiter {
        IC_8088& cpu;
        BusOp::Kind kind;
        uint32_t addr;
        bool await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            cpu.bus_op_ = {kind, addr & 0xFFFFF, 0, 0};
            cpu.eu_resume_ = h;
            return std::noop_coroutine();
        }
        uint8_t await_resume() noexcept { return cpu.bus_op_.read_data; }
    };

    // Bus write awaiter: sets bus_op_, suspends to BIU.
    struct BusWriteAwaiter {
        IC_8088& cpu;
        BusOp::Kind kind;
        uint32_t addr;
        uint8_t val;
        bool await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            cpu.bus_op_ = {kind, addr & 0xFFFFF, val, 0};
            cpu.eu_resume_ = h;
            return std::noop_coroutine();
        }
        void await_resume() noexcept {}
    };

    // INTA awaiter: posts INTA bus op, suspends, returns interrupt vector.
    struct IntaAwaiter {
        IC_8088& cpu;
        bool await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            cpu.bus_op_ = {BusOp::INTA, 0, 0, 0};
            cpu.eu_resume_ = h;
            return std::noop_coroutine();
        }
        uint8_t await_resume() noexcept { return cpu.bus_op_.read_data; }
    };

    // Halt awaiter: suspends EU back to BIU with no bus request.
    // Used by HLT instruction so the BIU can poll wake conditions.
    struct HaltAwaiter {
        IC_8088& cpu;
        bool await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            cpu.bus_op_.kind = BusOp::NONE;
            cpu.eu_resume_ = h;
            return std::noop_coroutine();
        }
        void await_resume() noexcept {}
    };

    // Idle awaiter: suspends EU to the BIU to burn N CLK cycles with the
    // bus passive. Models EU-internal execution time that has no bus
    // activity (e.g. per-iteration REP overhead).
    struct IdleAwaiter {
        IC_8088& cpu;
        uint32_t cycles;
        bool await_ready() noexcept { return cycles == 0; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            cpu.bus_op_ = {BusOp::IDLE, cycles, 0, 0};
            cpu.eu_resume_ = h;
            return std::noop_coroutine();
        }
        void await_resume() noexcept {}
    };

    // rmem8 awaiter: register shortcut (no suspend) or bus read (1 suspend).
    struct Rmem8Awaiter {
        IC_8088& cpu;
        uint32_t addr;
        bool is_reg() const noexcept { return addr >= REGS_BASE && addr < REGS_BASE + sizeof(cpu.regs_); }
        bool await_ready() noexcept { return is_reg(); }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            cpu.bus_op_ = {BusOp::MEM_READ, addr & 0xFFFFF, 0, 0};
            cpu.eu_resume_ = h;
            return std::noop_coroutine();
        }
        uint8_t await_resume() noexcept {
            if (is_reg()) return cpu.regs_[addr - REGS_BASE];
            return cpu.bus_op_.read_data;
        }
    };

    // wmem8 awaiter: register shortcut (no suspend) or bus write (1 suspend).
    struct Wmem8Awaiter {
        IC_8088& cpu;
        uint32_t addr;
        uint8_t val;
        bool is_reg() const noexcept { return addr >= REGS_BASE && addr < REGS_BASE + sizeof(cpu.regs_); }
        bool await_ready() noexcept {
            if (is_reg()) { cpu.regs_[addr - REGS_BASE] = val; return true; }
            return false;
        }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            cpu.bus_op_ = {BusOp::MEM_WRITE, addr & 0xFFFFF, val, 0};
            cpu.eu_resume_ = h;
            return std::noop_coroutine();
        }
        void await_resume() noexcept {}
    };

    // Inline awaiter-returning functions (no coroutine frame allocated)
    BusReadAwaiter eu_bus_read_byte(uint32_t addr) {
        return {*this, BusOp::MEM_READ, addr};
    }
    BusWriteAwaiter eu_bus_write_byte(uint32_t addr, uint8_t val) {
        return {*this, BusOp::MEM_WRITE, addr, val};
    }
    BusReadAwaiter eu_io_read_byte(uint16_t port) {
        return {*this, BusOp::IO_READ, port};
    }
    BusWriteAwaiter eu_io_write_byte(uint16_t port, uint8_t val) {
        return {*this, BusOp::IO_WRITE, (uint32_t)port, val};
    }
    Rmem8Awaiter rmem8(uint32_t addr) {
        return {*this, addr};
    }
    Wmem8Awaiter wmem8(uint32_t addr, uint8_t val) {
        return {*this, addr, val};
    }
    BusReadAwaiter fetch_byte(int offset) {
        return {*this, BusOp::FETCH, prefetch_base_ + (uint32_t)offset};
    }
    IdleAwaiter eu_idle(uint32_t cycles) {
        return {*this, cycles};
    }

    // --- Debugger read-only access (safe to call from any thread while paused) ---
    bool halted() const { return halted_; }
    void clear_halt() { halted_ = false; }
    bool breakpoint() const { return breakpoint_; }
    void clear_breakpoint() { breakpoint_ = false; }
    // Set the power-on CS:IP and apply a full CPU reset (test bench
    // only, on a powered-off CPU). Since the save-state refactor,
    // power-on does NOT reset register state -- it belongs to the
    // constructor (cold boot) or the archive (save-state load) -- so
    // overriding the vector must reset the live registers here.
    void set_reset_vector(uint16_t cs, uint16_t ip) {
        start_cs_ = cs;
        start_ip_ = ip;
        cpu_reset();
    }

    const uint16_t* regs16_ro() const { return reinterpret_cast<const uint16_t*>(regs_); }
    const uint8_t*  regs8_ro()  const { return regs_; }
    uint16_t ip()  const { return reg_ip_; }
    const uint16_t* ip_ptr() const { return &reg_ip_; }
    // 16-bit register indices
    enum Reg16 { AX=0, CX=1, DX=2, BX=3, SP=4, BP=5, SI=6, DI=7,
                 ES=8, CS=9, SS=10, DS=11 };
    // Flag byte offsets in regs8
    enum Flag { CF=40, PF=41, AF=42, ZF=43, SF=44, TF=45, IF=46, DF=47, OF=48 };
    TState t_state() const { return t_state_; }
    BusT bus_t() const { return bus_t_; }
    uint64_t instr_count() const { return instr_count_; }

#if BENCH_CFG_TRACE
    // Execution tracer. Non-owning; the board owns it and hands it here
    // before power-on. The CPU logs instruction flow and port reads; memory
    // writes are logged where they land (DRAM / ISA RAM), which is the only
    // point that sees DMA transfers as well as CPU stores.
    void set_tracer(CFGTracer* t) { tracer_ = t; }
    CFGTracer* tracer() const { return tracer_; }
#endif

    // Last bus transaction (for debugger bus analyzer)
    struct BusTx {
        uint32_t addr = 0;
        uint8_t data = 0;
        uint8_t type = 7;  // BUS_PASSIVE
    };
    const BusTx& last_bus_tx() const { return last_bus_tx_; }

    void save(cereal::BinaryOutputArchive& ar) override { serialize(ar); }
    // load: not used -- Board reconstructs via IC_8088(archive) constructor

    template <class Archive> void serialize(Archive& ar) {
        ar(regs_, reg_ip_, i_rm_, i_w_, i_reg_, i_mod_, i_mod_size_, i_d_,
           i_reg4bit_, raw_opcode_id_, xlat_opcode_id_, extra_,
           rep_mode_, seg_override_en_, rep_override_en_, trap_flag_,
           div_error_, seg_override_, op_source_, op_dest_, rm_addr_,
           op_to_addr_, op_from_addr_, i_data0_, i_data1_, i_data2_,
           i_imm_offset_, scratch_uint_, scratch2_uint_, op_result_,
           scratch_int_, scratch_uchar_, set_flags_type_,
           bus_t_, t_state_, nmi_pending_, nmi_prev_, halted_, breakpoint_,
           int13_pending_, int13_ret_cs_, int13_ret_ip_, int13_ret_sp_,
           last_bus_tx_.addr, last_bus_tx_.data, last_bus_tx_.type,
           instr_count_, start_cs_, start_ip_, prefetch_base_);
    }

protected:
    void on_cycle(Fiber caller) override;

private:
    // ---- BIU coroutine ----
    BIUTask biu_run();

    // ---- EU coroutine (single frame -- execute() inlined into eu_run) ----
    EUTask<void> eu_run();

    // ---- Pure ALU (not coroutines) ----
    void check_nmi();
    void cpu_reset();
    void set_opcode(uint8_t opcode);
    void make_flags();
    void set_flags(int new_flags);
    int  AAA_AAS(int which_operation);
    void set_AF_OF_arith();
    int  set_CF(int new_CF);
    int  set_AF(int new_AF);
    int  set_OF(int new_OF);
    void decode_rm_reg();
    uint32_t get_reg_addr(int reg_id);
    int  top_bit();
    int  sign_of(int val);
    void index_inc(int reg_id);

    // ---- BIU pin driving ----
    void drive_address(uint32_t address);
    void drive_data(uint8_t value);
    uint8_t read_data();
    void release_data();
    void drive_status(uint8_t s2, uint8_t s1, uint8_t s0);
    void drive_status_passive();

    // ---- Shared EU/BIU state ----
    BusOp bus_op_;
    std::coroutine_handle<> eu_resume_{std::noop_coroutine()};
    bool eu_done_ = false;
    BIUTask biu_task_{};

    // ---- Pins ----

    // Multiplexed address/data: AD0=pin16 .. AD7=pin9
    Pin pin_ad_[8];

    // Upper address: A8=pin8 .. A14=pin2, A15=pin39 .. A19=pin35
    Pin pin_a_upper_[12];  // A8..A19

    // Status outputs to 8288
    Pin pin_s0_;    // Pin 26: ~S0
    Pin pin_s1_;    // Pin 27: ~S1
    Pin pin_s2_;    // Pin 28: ~S2

    // Queue status outputs
    Pin pin_qs0_;   // Pin 25: QS0
    Pin pin_qs1_;   // Pin 24: QS1

    // Control inputs
    Pin pin_clk_;   // Pin 19: CLK
    Pin pin_reset_; // Pin 21: RESET
    Pin pin_ready_; // Pin 22: READY
    Pin pin_intr_;  // Pin 18: INTR
    Pin pin_nmi_;   // Pin 17: NMI
    Pin pin_test_;  // Pin 23: ~TEST
    Pin pin_vcc_;   // Pin 31/40: VCC

    // Outputs
    Pin pin_lock_;  // Pin 29: ~LOCK
    Pin pin_rqgt0_; // Pin 30: ~RQ/~GT0

    // ---- CPU state ----
    static constexpr uint32_t REGS_BASE = 0xF0000;

    uint8_t regs_[64] = {};
    uint16_t* regs16() { return reinterpret_cast<uint16_t*>(regs_); }
    uint8_t*  regs8()  { return regs_; }
    uint16_t reg_ip_ = 0;

    uint8_t i_rm_ = 0, i_w_ = 0, i_reg_ = 0, i_mod_ = 0;
    uint8_t i_mod_size_ = 0, i_d_ = 0, i_reg4bit_ = 0;
    uint8_t raw_opcode_id_ = 0, xlat_opcode_id_ = 0, extra_ = 0;
    uint8_t rep_mode_ = 0, seg_override_en_ = 0, rep_override_en_ = 0;
    uint8_t trap_flag_ = 0;
    bool div_error_ = false;
    uint16_t seg_override_ = 0;

    uint32_t op_source_ = 0, op_dest_ = 0, rm_addr_ = 0;
    uint32_t op_to_addr_ = 0, op_from_addr_ = 0;
    uint32_t i_data0_ = 0, i_data1_ = 0, i_data2_ = 0;
    int i_imm_offset_ = 2;
    uint32_t scratch_uint_ = 0, scratch2_uint_ = 0;
    int      op_result_ = 0, scratch_int_ = 0;
    uint8_t  scratch_uchar_ = 0;
    uint32_t set_flags_type_ = 0;

    BusT bus_t_ = BusT::T1;
    TState t_state_ = TState::Ti;

    bool nmi_pending_ = false;
    Level nmi_prev_ = Level::HiZ;
    bool halted_ = false;
    bool breakpoint_ = false;
    uint32_t last_logged_cs_ip_ = ~0u;

public:
    // Debug: direct memory peek (set externally, bypasses bus)
    std::function<uint8_t(uint32_t)> debug_peek_;
    // Debug: returns bus state string (AEN, ~MEMW, ~MEMR, inhibit, etc.)
    std::function<std::string()> debug_bus_state_;
private:

    bool int13_pending_ = false;
    uint16_t int13_ret_cs_ = 0;
    uint16_t int13_ret_ip_ = 0;
    uint16_t int13_ret_sp_ = 0;

    BusTx last_bus_tx_;
    uint64_t instr_count_ = 0;

#if BENCH_CFG_TRACE
    CFGTracer* tracer_ = nullptr;

    // Pending interrupt-handler stack watermarks. See PC_INTERRUPT_.
    static constexpr int kIntSpMax = 16;
    uint16_t int_sp_[kIntSpMax] = {};
    uint16_t int_ss_[kIntSpMax] = {};
    uint8_t  int_sp_top_ = 0;

    // True while the CPU is inside an interrupt handler. Pops watermarks the
    // stack has already unwound past, so it self-corrects regardless of how
    // the handler returned -- IRET, RETF n, or a stack fixup and a jump.
    bool in_int_handler() {
        // Reg16::SS / Reg16::SP -- the file-scope REG_* constants used by the
        // macros in this header live in the .cpp and are not visible here.
        //
        // A watermark is retired when the stack has unwound past it, OR when
        // SS no longer matches the stack it was taken on -- that stack is
        // gone, so the watermark can never be reached and holding it would
        // latch the gate on forever. (POST takes interrupts on the BIOS
        // stack; the boot sector then does MOV SS,0020 and never returns to
        // it.)
        while (int_sp_top_) {
            const uint16_t sp = regs16()[Reg16::SP];
            const uint16_t ss = regs16()[Reg16::SS];
            const int top = int_sp_top_ - 1;
            if (ss != int_ss_[top] || sp > int_sp_[top])
                --int_sp_top_;
            else
                break;
        }
        return int_sp_top_ != 0;
    }
#endif

    uint16_t start_cs_, start_ip_;
    uint32_t prefetch_base_ = 0;

    Scheduler* scheduler_ = nullptr;

    // Decode tables (from 8086tiny bios.asm).
    static constexpr uint8_t TABLE[20][256] = {
        // [0] rm_mode12_reg1
        {3,3,5,5,6,7,5,3},
        // [1] rm_mode012_reg2
        {6,7,6,7,12,12,12,12},
        // [2] rm_mode12_disp
        {1,1,1,1,1,1,1,1},
        // [3] rm_mode12_dfseg
        {11,11,10,10,11,11,10,11},
        // [4] rm_mode0_reg1
        {3,3,5,5,6,7,12,3},
        // [5] rm_mode012_reg2 (same data as [1])
        {6,7,6,7,12,12,12,12},
        // [6] rm_mode0_disp
        {0,0,0,0,0,0,1,0},
        // [7] rm_mode0_dfseg
        {11,11,10,10,11,11,11,11},
        // [8] xlat_ids
        {9,9,9,9,7,7,25,26, 9,9,9,9,7,7,25,26, 9,9,9,9,7,7,25,26, 9,9,9,9,7,7,25,26,
         9,9,9,9,7,7,27,28, 9,9,9,9,7,7,27,28, 9,9,9,9,7,7,27,29, 9,9,9,9,7,7,27,29,
         2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,
         51,54,52,52,52,52,52,52, 55,55,55,55,52,52,52,52, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         8,8,8,8,15,15,24,24, 9,9,9,9,10,10,10,10, 16,16,16,16,16,16,16,16, 30,31,32,53,33,34,35,36,
         11,11,11,11,17,17,18,18, 47,47,17,17,17,17,18,18, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
         12,12,19,19,37,37,20,20, 49,50,19,19,38,39,40,19, 12,12,12,12,41,42,43,44, 54,54,54,54,54,54,54,54,
         13,13,13,13,21,21,22,22, 14,14,14,14,21,21,22,22, 54,54,23,23,53,45,6,6, 46,46,46,46,46,46,5,5},
        // [9] ex_data
        {0,0,0,0,0,0,8,8, 1,1,1,1,1,1,9,9, 2,2,2,2,2,2,10,10, 3,3,3,3,3,3,11,11,
         4,4,4,4,4,4,8,0, 5,5,5,5,5,5,9,1, 6,6,6,6,6,6,10,2, 7,7,7,7,7,7,11,0,
         0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         0,0,21,21,21,21,21,21, 0,0,0,0,21,21,21,21, 21,21,21,21,21,21,21,21, 21,21,21,21,21,21,21,21,
         0,0,0,0,0,0,0,0, 8,8,8,8,12,12,12,12, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,255,0,
         0,0,0,0,0,0,0,0, 0,0,1,1,2,2,1,1, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         1,1,0,0,16,22,0,0, 0,0,1,1,0,255,48,2, 0,0,0,0,255,255,40,11, 3,3,3,3,3,3,3,3,
         43,43,43,43,0,0,0,0, 0,0,0,0,1,1,1,1, 1,21,0,0,2,40,21,21, 80,81,92,93,94,95,0,0},
        // [10] std_flags
        {3,3,3,3,3,3,0,0, 5,5,5,5,5,5,0,0, 1,1,1,1,1,1,0,0, 1,1,1,1,1,1,0,0,
         5,5,5,5,5,5,0,1, 3,3,3,3,3,3,0,1, 5,5,5,5,5,5,0,1, 3,3,3,3,3,3,0,1,
         1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         1,1,1,1,5,5,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0, 5,5,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,5,5,0,0, 0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
        // [11] parity (1 = even parity)
        {1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1, 0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0,
         0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0, 1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1,
         0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0, 1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1,
         1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1, 0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0,
         0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0, 1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1,
         1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1, 0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0,
         1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1, 0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0,
         0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0, 1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1},
        // [12] base_size
        {2,2,2,2,1,1,1,1, 2,2,2,2,1,1,1,1, 2,2,2,2,1,1,1,1, 2,2,2,2,1,1,1,1,
         2,2,2,2,1,1,1,1, 2,2,2,2,1,1,1,1, 2,2,2,2,1,1,1,1, 2,2,2,2,1,1,1,1,
         1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
         1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
         2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 1,1,1,1,1,1,1,1, 1,1,0,1,1,1,1,1,
         3,3,3,3,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
         3,3,0,0,2,2,2,2, 4,1,0,0,0,0,0,0, 2,2,2,2,2,2,1,1, 2,2,2,2,2,2,2,2,
         2,2,2,2,2,2,2,2, 0,0,0,0,1,1,1,1, 1,2,1,1,1,1,2,2, 1,1,1,1,1,1,2,2},
        // [13] i_w_adder
        {0,0,0,0,1,1,0,0, 0,0,0,0,1,1,0,0, 0,0,0,0,1,1,0,0, 0,0,0,0,1,1,0,0,
         0,0,0,0,1,1,0,0, 0,0,0,0,1,1,0,0, 0,0,0,0,1,1,0,0, 0,0,0,0,1,1,0,0,
         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0, 1,1,1,1,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         1,1,1,1,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0, 1,1,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
         0,0,0,0,0,0,1,1, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
        // [14] i_mod_adder
        {1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0,
         1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0, 1,1,1,1,0,0,0,0,
         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         1,1,0,0,1,1,1,1, 0,0,0,0,0,0,0,0, 1,1,1,1,0,0,0,0, 1,1,1,1,1,1,1,1,
         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,1,1, 0,0,0,0,0,0,1,1},
        // [15] jxx_dec_a
        {48,40,43,40,44,41,49,49},
        // [16] jxx_dec_b
        {49,49,49,43,49,49,49,43},
        // [17] jxx_dec_c
        {49,49,49,49,49,49,44,44},
        // [18] jxx_dec_d
        {49,49,49,49,49,49,48,48},
        // [19] flags_mult
        {0,2,4,6,7,8,9,10,11},
    };
};

// ========================================================================
// Macros: expand inline in eu_run(), zero coroutine frames.
// These use co_await and reference IC_8088 members -- only valid
// inside IC_8088 coroutine member functions.
// ========================================================================

#define RMEM16_(addr_, dest_) do { \
    uint32_t _a16 = (addr_); \
    if (_a16 >= REGS_BASE && _a16 < REGS_BASE + 63) { \
        uint32_t _off = _a16 - REGS_BASE; \
        (dest_) = (uint32_t)(uint16_t)(regs_[_off] | (regs_[_off + 1] << 8)); \
    } else { \
        uint8_t _lo16 = co_await eu_bus_read_byte(_a16); \
        uint8_t _hi16 = co_await eu_bus_read_byte(_a16 + 1); \
        (dest_) = (uint32_t)(uint16_t)(_lo16 | (_hi16 << 8)); \
    } \
} while(0)

#define WMEM16_(addr_, val_) do { \
    uint32_t _wa16 = (addr_); uint16_t _wv16 = (uint16_t)(val_); \
    if (_wa16 >= REGS_BASE && _wa16 < REGS_BASE + 63) { \
        uint32_t _off = _wa16 - REGS_BASE; \
        regs_[_off] = _wv16 & 0xFF; regs_[_off + 1] = (_wv16 >> 8) & 0xFF; \
    } else { \
        co_await eu_bus_write_byte(_wa16, _wv16 & 0xFF); \
        co_await eu_bus_write_byte(_wa16 + 1, (_wv16 >> 8) & 0xFF); \
    } \
} while(0)

#define RMEM_(addr_, dest_) do { \
    if (i_w_) { RMEM16_(addr_, dest_); } \
    else { (dest_) = (uint32_t)co_await rmem8(addr_); } \
} while(0)

#define WMEM_(addr_, val_) do { \
    if (i_w_) { WMEM16_(addr_, (uint16_t)(val_)); } \
    else { co_await wmem8(addr_, (uint8_t)(val_)); } \
} while(0)

#define PUSH16_(val_) do { \
    uint16_t _pv = (uint16_t)(val_); \
    regs16()[REG_SP] -= 2; \
    uint32_t _pa = 16u * regs16()[REG_SS] + regs16()[REG_SP]; \
    co_await eu_bus_write_byte(_pa, _pv & 0xFF); \
    co_await eu_bus_write_byte(_pa + 1, (_pv >> 8) & 0xFF); \
} while(0)

#define POP16_(dest_) do { \
    uint32_t _pa = 16u * regs16()[REG_SS] + regs16()[REG_SP]; \
    uint8_t _plo = co_await eu_bus_read_byte(_pa); \
    uint8_t _phi = co_await eu_bus_read_byte(_pa + 1); \
    regs16()[REG_SP] += 2; \
    (dest_) = (uint16_t)(_plo | (_phi << 8)); \
} while(0)

#define FETCH_WORD_(offset_, dest_) do { \
    uint8_t _flo = co_await fetch_byte(offset_); \
    uint8_t _fhi = co_await fetch_byte((offset_) + 1); \
    (dest_) = (uint16_t)(_flo | (_fhi << 8)); \
} while(0)

#define PC_INTERRUPT_(int_num_) do { \
    uint8_t _intv = (int_num_); \
    set_opcode(0xCD); make_flags(); \
    PUSH16_((uint16_t)scratch_uint_); \
    PUSH16_(regs16()[REG_CS]); \
    PUSH16_(reg_ip_); \
    uint8_t _ilo = co_await eu_bus_read_byte(4u * _intv); \
    uint8_t _ihi = co_await eu_bus_read_byte(4u * _intv + 1); \
    uint8_t _clo = co_await eu_bus_read_byte(4u * _intv + 2); \
    uint8_t _chi = co_await eu_bus_read_byte(4u * _intv + 3); \
    regs16()[REG_CS] = (uint16_t)(_clo | (_chi << 8)); \
    reg_ip_ = (uint16_t)(_ilo | (_ihi << 8)); \
    regs8()[FLAG_TF] = 0; \
    regs8()[FLAG_IF] = 0; \
    INT_DEPTH_ENTER_(); \
} while(0)

// Interrupt-handler detection for the execution tracer.
//
// Every path into a handler on a real 8088 -- INTR, NMI, INT n, INT 3, INTO,
// divide error, trap flag -- goes through PC_INTERRUPT_, which pushes flags,
// CS and IP: six bytes. Recording SS:SP at that moment gives a watermark, and
// the handler has finished once SP has risen back to it.
//
// Counting IRETs instead does not work: this trace executes 111176 INTs but
// only 7681 IRETs, because BIOS service handlers commonly return with RETF n
// after adjusting the stack. A watermark does not care how the handler left.
//
// SS is captured too, since a handler that switches stacks would otherwise
// compare SP against an unrelated stack. Nesting works without a counter --
// the outermost watermark is the lowest, so the deepest one still pending is
// what matters, and a small stack of them is kept.
//
// While a handler is active the tracer records the execution timeline but
// builds no CFG node or edge. Otherwise a handler entered between two
// instructions looks like a control-flow successor of whatever it
// interrupted: edges that never existed, conditional jumps with impossible
// fan-out, and straight-line code fragmented into single-instruction blocks.
#if BENCH_CFG_TRACE
#define INT_DEPTH_ENTER_() do { \
    if (int_sp_top_ < kIntSpMax) { \
        int_sp_[int_sp_top_] = regs16()[REG_SP]; \
        int_ss_[int_sp_top_] = regs16()[REG_SS]; \
        ++int_sp_top_; \
    } \
} while(0)
#else
#define INT_DEPTH_ENTER_() do { } while(0)
#endif

} // namespace bench
