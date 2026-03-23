#pragma once
#include "core/fiber_component.h"
#include "board/socket.h"

namespace bench {

class Scheduler;

// Intel 8088 CPU (maximum mode).
//
// 40-pin DIP. U3 on the 5150 motherboard.
//
// Pin functions (from BRD):
//   Pin  1: GND
//   Pin  2-8: A14-A8 (address bus, active during T1)
//   Pin  9-16: AD7-AD0 (multiplexed address/data bus)
//   Pin 17: NMI (input, non-maskable interrupt, rising-edge triggered)
//   Pin 18: INTR (input, maskable interrupt request)
//   Pin 19: CLK (input, clock from 8284A)
//   Pin 20: GND
//   Pin 21: RESET (input, from 8284A)
//   Pin 22: READY (input, from 8284A)
//   Pin 23: ~TEST (input, from 8087 BUSY)
//   Pin 24: QS1 (output, queue status)
//   Pin 25: QS0 (output, queue status)
//   Pin 26: ~S0 (output, bus cycle status -> 8288)
//   Pin 27: ~S1 (output, bus cycle status -> 8288)
//   Pin 28: ~S2 (output, bus cycle status -> 8288)
//   Pin 29: ~LOCK (output, bus lock prefix)
//   Pin 30: ~RQ/~GT0 (I/O, bus request/grant for 8087)
//   Pin 31: VCC (+5V)
//   Pin 32: (no connect in BRD -- MN/~MX tied to GND externally)
//   Pin 33: GND (MN/~MX = GND -> maximum mode)
//   Pin 34: (no connect)
//   Pin 35-39: A19-A15 (address bus)
//   Pin 40: VCC (+5V)
//
// Bus cycle status encoding (active low, ~S2/~S1/~S0):
//   0,0,0 = INTA       0,0,1 = IOR
//   0,1,0 = IOW        0,1,1 = Halt
//   1,0,0 = Opcode fetch  1,0,1 = Memory read
//   1,1,0 = Memory write  1,1,1 = Passive (no bus cycle)
//
// Threading: Active IC (fiber). Overrides run() with instruction execution loop.
//            Yields until VCC goes High, then executes instructions.
//            Each instruction step drives bus signals for memory/IO access.
class IC_8088 : public FiberComponent {
public:
    // Bus cycle phase (actual T-state visible to debugger)
    enum class TState : uint8_t { Ti, T1, T2, T3, Tw, T4 };
    // Bidir direction hint (for DAG permutation selection, internal)
    enum class BusT : uint8_t { T1, T2_Read, T2_Write };

    IC_8088(uint16_t start_cs = 0xF000, uint16_t start_ip = 0x0100);

    void install(Socket& socket);
    void set_scheduler(Scheduler* s) { scheduler_ = s; }

protected:
    void run() override;

private:
    void check_nmi();
    // --- Bus operations ---
    uint8_t bus_read_byte(uint32_t address);
    void bus_write_byte(uint32_t address, uint8_t value);
    uint16_t bus_read_word(uint32_t address);
    void bus_write_word(uint32_t address, uint16_t value);
    uint8_t io_read_byte(uint16_t port);
    void io_write_byte(uint16_t port, uint8_t value);

    void drive_address(uint32_t address);
    void drive_data(uint8_t value);
    uint8_t read_data();
    void release_data();
    void drive_status(uint8_t s2, uint8_t s1, uint8_t s0);
    void drive_status_passive();
    void full_wait_clk(const char* stateYield);

    // --- Memory routing (register file or bus) ---
    static constexpr uint32_t REGS_BASE = 0xF0000;

    uint8_t rmem8(uint32_t addr);
    uint16_t rmem16(uint32_t addr);
    void wmem8(uint32_t addr, uint8_t val);
    void wmem16(uint32_t addr, uint16_t val);
    uint32_t rmem(uint32_t addr);   // byte or word based on i_w_
    void wmem(uint32_t addr, uint32_t val);

    // --- Stack ---
    void push16(uint16_t val);
    uint16_t pop16();

    // --- CPU core (ported from 8086tiny) ---
    void cpu_reset();
    void execute();
    void set_opcode(uint8_t opcode);
    void pc_interrupt(uint8_t interrupt_num);
    void make_flags();
    void set_flags(int new_flags);
    int  AAA_AAS(int which_operation);
    void set_AF_OF_arith();
    int  set_CF(int new_CF);
    int  set_AF(int new_AF);
    int  set_OF(int new_OF);

    // --- Decode helpers ---
    void decode_rm_reg();
    uint32_t get_reg_addr(int reg_id);
    int top_bit();
    int sign_of(int val);
    void index_inc(int reg_id);

    // Instruction fetch
    uint8_t fetch_byte(int offset);
    uint16_t fetch_word(int offset);
    uint8_t prefetch_[6] = {};
    int prefetch_len_ = 0;
    uint32_t prefetch_base_ = 0;

    // --- Pin handles ---

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

    // --- CPU state (from 8086tiny, adapted) ---

    // Registers: 16-bit regs at byte offsets 0-27 (14 regs x 2 bytes),
    // individual flag bytes at offsets 40-48 (FLAG_CF..FLAG_OF).
    uint8_t regs_[64] = {};

    uint16_t* regs16() { return reinterpret_cast<uint16_t*>(regs_); }
    uint8_t*  regs8()  { return regs_; }

    uint16_t reg_ip_ = 0;

    // Instruction decode state
    uint8_t i_rm_ = 0, i_w_ = 0, i_reg_ = 0, i_mod_ = 0;
    uint8_t i_mod_size_ = 0, i_d_ = 0, i_reg4bit_ = 0;
    uint8_t raw_opcode_id_ = 0, xlat_opcode_id_ = 0, extra_ = 0;
    uint8_t rep_mode_ = 0, seg_override_en_ = 0, rep_override_en_ = 0;
    uint8_t trap_flag_ = 0;
    bool div_error_ = false;
    uint16_t seg_override_ = 0;

    // Operand state
    uint32_t op_source_ = 0, op_dest_ = 0, rm_addr_ = 0;
    uint32_t op_to_addr_ = 0, op_from_addr_ = 0;
    uint32_t i_data0_ = 0, i_data1_ = 0, i_data2_ = 0;
    int i_imm_offset_ = 2;
    uint32_t scratch_uint_ = 0, scratch2_uint_ = 0;
    int      op_result_ = 0, scratch_int_ = 0;
    uint8_t  scratch_uchar_ = 0;
    uint32_t set_flags_type_ = 0;

    // Instruction decode tables (from 8086tiny bios.asm).
    // 20 tables x 256 bytes. Small tables (R/M, jxx, flags) are padded with zeros.
    // Index: [0] rm_mode12_reg1, [1] rm_mode012_reg2, [2] rm_mode12_disp,
    //        [3] rm_mode12_dfseg, [4] rm_mode0_reg1, [5] rm_mode012_reg2,
    //        [6] rm_mode0_disp, [7] rm_mode0_dfseg, [8] xlat_ids, [9] ex_data,
    //        [10] std_flags, [11] parity, [12] base_size, [13] i_w_adder,
    //        [14] i_mod_adder, [15] jxx_dec_a, [16] jxx_dec_b, [17] jxx_dec_c,
    //        [18] jxx_dec_d, [19] flags_mult
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
        {9,9,9,9,7,7,25,26, 9,9,9,9,7,7,25,48, 9,9,9,9,7,7,25,26, 9,9,9,9,7,7,25,26,
         9,9,9,9,7,7,27,28, 9,9,9,9,7,7,27,28, 9,9,9,9,7,7,27,29, 9,9,9,9,7,7,27,29,
         2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,
         51,54,52,52,52,52,52,52, 55,55,55,55,52,52,52,52, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
         8,8,8,8,15,15,24,24, 9,9,9,9,10,10,10,10, 16,16,16,16,16,16,16,16, 30,31,32,53,33,34,35,36,
         11,11,11,11,17,17,18,18, 47,47,17,17,17,17,18,18, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
         12,12,19,19,37,37,20,20, 49,50,19,19,38,39,40,19, 12,12,12,12,41,42,43,44, 53,53,53,53,53,53,53,53,
         13,13,13,13,21,21,22,22, 14,14,14,14,21,21,22,22, 53,0,23,23,53,45,6,6, 46,46,46,46,46,46,5,5},
        // [9] ex_data
        {0,0,0,0,0,0,8,8, 1,1,1,1,1,1,9,36, 2,2,2,2,2,2,10,10, 3,3,3,3,3,3,11,11,
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
        {2,2,2,2,1,1,1,1, 2,2,2,2,1,1,1,2, 2,2,2,2,1,1,1,1, 2,2,2,2,1,1,1,1,
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

    // Bus T-state tracking for DAG bidir blocks.
    // T1: driving AD (address) + S0-S2 (status)  -> AD=Output, S0-S2=Output
    // T2_READ: AD released (input), S0-S2 passive -> AD=Input,  S0-S2=HiZ
    // T2_WRITE: AD driving (data), S0-S2 passive  -> AD=Output, S0-S2=HiZ
    // T3/T4/Tw: same as T2 for their respective read/write direction
    BusT bus_t_ = BusT::T1;  // CPU starts by fetching -- first action is T1
    TState t_state_ = TState::Ti;  // actual bus cycle phase (for debugger)

    // Interrupt state
    bool nmi_pending_ = false;

    // NMI edge tracking
    Level nmi_prev_ = Level::HiZ;

    // Halted flag -- set when CPU reaches HLT or CS:IP = 0:0
    bool halted_ = false;
public:
    bool halted() const { return halted_; }
    void clear_halt() { halted_ = false; }
    void set_reset_vector(uint16_t cs, uint16_t ip) { start_cs_ = cs; start_ip_ = ip; }

    // --- Debugger read-only access (safe to call from any thread while paused) ---
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
private:

    // Start address (set via constructor, applied in cpu_reset)
    uint16_t start_cs_;
    uint16_t start_ip_;

    Scheduler* scheduler_ = nullptr;
};

} // namespace bench
