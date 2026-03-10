#pragma once
#include "core/component.h"
#include "board/socket.h"

namespace bench {

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
// Bus cycle status encoding (active low, accent on ~S2/~S1/~S0):
//   0,0,0 = INTA       0,0,1 = IOR
//   0,1,0 = IOW        0,1,1 = Halt
//   1,0,0 = Opcode fetch  1,0,1 = Memory read
//   1,1,0 = Memory write  1,1,1 = Passive (no bus cycle)
//
// Threading: Active IC. Overrides run() with instruction execution loop.
//            Blocks on wait_mailbox() until VCC goes High.
//            Each instruction step drives bus signals for memory/IO access.
class IC_8088 : public Component {
public:
    IC_8088();

    void install(Socket& socket);

protected:
    void run(std::stop_token stop) override;
    void on_signal_change(Signal& signal, Level old_level, Level new_level) override;

private:
    // --- Bus operations (accent: every mem[] access goes through these) ---
    uint8_t bus_read_byte(uint32_t address);
    void bus_write_byte(uint32_t address, uint8_t value);
    uint16_t bus_read_word(uint32_t address);
    void bus_write_word(uint32_t address, uint16_t value);
    uint8_t io_read_byte(uint16_t port);
    void io_write_byte(uint16_t port, uint8_t value);

    // Drive the 20-bit address onto A0-A19 (pins 2-8, 9-16 low byte, 35-39)
    void drive_address(uint32_t address);
    // Drive 8-bit data onto AD0-AD7
    void drive_data(uint8_t value);
    // Read 8-bit data from AD0-AD7
    uint8_t read_data();
    // Release AD0-AD7 (tri-state)
    void release_data();
    // Drive S0/S1/S2 bus cycle status
    void drive_status(uint8_t s2, uint8_t s1, uint8_t s0);
    // Set passive (no bus cycle) on status lines
    void drive_status_passive();

    // Wait for CLK edge
    void wait_clk_rising();
    void wait_clk_falling();

    // --- CPU core (ported from 8086tiny) ---
    void cpu_reset();
    void execute();                     // single instruction
    void set_opcode(uint8_t opcode);
    void pc_interrupt(uint8_t interrupt_num);
    void make_flags();
    void set_flags(int new_flags);
    int  AAA_AAS(int which_operation);
    void set_AF_OF_arith();
    int  set_CF(int new_CF);
    int  set_AF(int new_AF);
    int  set_OF(int new_OF);

    // --- Pin pointers ---

    // Multiplexed address/data: AD0=pin16 .. AD7=pin9
    Signal* pin_ad_[8] = {};

    // Upper address: A8=pin8 .. A14=pin2, A15=pin39 .. A19=pin35
    Signal* pin_a_upper_[12] = {};  // A8..A19

    // Status outputs to 8288
    Signal* pin_s0_  = nullptr;   // Pin 26: ~S0
    Signal* pin_s1_  = nullptr;   // Pin 27: ~S1
    Signal* pin_s2_  = nullptr;   // Pin 28: ~S2

    // Queue status outputs
    Signal* pin_qs0_ = nullptr;   // Pin 25: QS0
    Signal* pin_qs1_ = nullptr;   // Pin 24: QS1

    // Control inputs
    Signal* pin_clk_   = nullptr; // Pin 19: CLK
    Signal* pin_reset_ = nullptr; // Pin 21: RESET
    Signal* pin_ready_ = nullptr; // Pin 22: READY
    Signal* pin_intr_  = nullptr; // Pin 18: INTR
    Signal* pin_nmi_   = nullptr; // Pin 17: NMI
    Signal* pin_test_  = nullptr; // Pin 23: ~TEST
    Signal* pin_vcc_   = nullptr; // Pin 31/40: VCC

    // Outputs
    Signal* pin_lock_  = nullptr; // Pin 29: ~LOCK
    Signal* pin_rqgt0_ = nullptr; // Pin 30: ~RQ/~GT0

    // --- CPU state (from 8086tiny, adapted) ---

    // Registers stored as an array, accessed via regs16/regs8 indices.
    // 8086tiny stores 16-bit regs at byte offsets 0-31 (16 regs x 2 bytes),
    // and individual flag bytes at offsets 40-48 (FLAG_CF..FLAG_OF).
    // Sized to 64 bytes to cover all indices.
    uint8_t regs_[64] = {};

    // Convenience pointers into regs_[]
    uint16_t* regs16() { return reinterpret_cast<uint16_t*>(regs_); }
    uint8_t*  regs8()  { return regs_; }

    uint16_t reg_ip_ = 0;

    // Instruction decode state
    uint8_t i_rm_ = 0, i_w_ = 0, i_reg_ = 0, i_mod_ = 0;
    uint8_t i_mod_size_ = 0, i_d_ = 0, i_reg4bit_ = 0;
    uint8_t raw_opcode_id_ = 0, xlat_opcode_id_ = 0, extra_ = 0;
    uint8_t rep_mode_ = 0, seg_override_en_ = 0, rep_override_en_ = 0;
    uint8_t trap_flag_ = 0;
    uint16_t seg_override_ = 0;

    // Operand state
    uint32_t op_source_ = 0, op_dest_ = 0, rm_addr_ = 0;
    uint32_t op_to_addr_ = 0, op_from_addr_ = 0;
    uint32_t i_data0_ = 0, i_data1_ = 0, i_data2_ = 0;
    uint32_t scratch_uint_ = 0, scratch2_uint_ = 0;
    int      op_result_ = 0, scratch_int_ = 0;
    uint8_t  scratch_uchar_ = 0;
    uint32_t set_flags_type_ = 0;

    // BIOS decode tables (loaded from ROM area after reset)
    uint8_t bios_table_[20][256] = {};

    // Interrupt state
    bool nmi_pending_ = false;
    bool nmi_prev_ = false;   // for edge detection

    // CLK tracking for bus cycle timing
    volatile bool clk_level_ = false;
    std::binary_semaphore clk_sem_{0};
    bool clk_waited_rising_ = false;
};

} // namespace bench
