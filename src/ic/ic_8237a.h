#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

class IC_74S245;

// Intel 8237A-5 DMA Controller.
//
// 40-pin DIP. 4-channel DMA controller with auto-initialize, single/block/
// demand transfer modes, and priority rotation.
//
// Pin functions (from BRD, U35 on 5150):
//   Pin  1: ~IOR    (I/O, I/O read strobe)
//   Pin  2: ~IOW    (I/O, I/O write strobe)
//   Pin  3: ~MEMR   (output, memory read during DMA)
//   Pin  4: ~MEMW   (output, memory write during DMA)
//   Pin  5: VCC     (+5V)
//   Pin  6: READY   (input, memory/IO ready)
//   Pin  7: HLDA    (input, hold acknowledge from CPU)
//   Pin  8: ADSTB   (output, address strobe -- latches upper address)
//   Pin  9: AEN     (output, address enable -- active during DMA)
//   Pin 10: HRQ     (output, hold request to CPU)
//   Pin 11: ~CS     (input, chip select)
//   Pin 12: CLK     (input, clock)
//   Pin 13: RESET   (input)
//   Pin 14-15: ~DACK2, ~DACK3  (output)
//   Pin 16-19: DREQ3-DREQ0     (input)
//   Pin 20: GND
//   Pin 21-23,26-30: DB7-DB0   (bidirectional data bus)
//   Pin 24-25: ~DACK1, ~DACK0  (output)
//   Pin 31: VCC
//   Pin 32-35: A0-A3           (I/O, address / register select)
//   Pin 36: ~EOP               (I/O, end of process / terminal count)
//   Pin 37-40: A4-A7           (output, upper address during DMA)
//
// DMA transfer cycle (from datasheet Figure 11):
//   S1: AEN high, drive A0-A7 (lower addr), DB0-DB7 (upper addr A8-A15),
//       ADSTB high, ~DACK asserted
//   S2: ADSTB falls (latches upper addr in 74S373), release DB,
//       assert ~MEMR/~MEMW strobes
//   S3: Wait state (extends read pulse; skipped in compressed timing)
//   S4: Transfer completes, deassert strobes, update addr/count, TC check
//
// 5150 usage:
//   CH0: DRAM refresh (auto-init, single transfer, DREQ0 from PIT ch1)
//   CH1-CH3: available for ISA peripherals
//
// Callback IC -- never yields, completes all work in on_signal_change().
class IC_8237A : public CallbackComponent {
public:
    IC_8237A();

    void install(Socket& socket);

    // Transceivers the DMA controller nudges during transfers:
    //   U8  (AD<->D):  HiZ during DMA (CPU disconnected)
    //   U12 (D<->MD):  data path to/from DRAM
    //   U13 (D<->XD):  data path to/from ISA bus
    //   U14 (cmd):     B->A so DMA's ~MEMR/~MEMW reach system side
    void set_xcvr(IC_74S245* u8, IC_74S245* u12, IC_74S245* u13, IC_74S245* u14);

protected:
    void on_signal_change(Fiber caller) override;

private:
    void on_bus_write();
    void on_bus_read();
    void on_reset();
    void on_clk_falling();
    void evaluate_dreq();
    void end_dma_service();
    void drive_data(uint8_t value);
    void release_data();
    void release_address();
    uint8_t read_data() const;
    bool is_dma_active() const {
        return state_ == State::S1 || state_ == State::S2 ||
               state_ == State::S3 || state_ == State::S4 ||
               state_ == State::M2M_S1 || state_ == State::M2M_S2 ||
               state_ == State::M2M_S3 || state_ == State::M2M_S4;
    }

    // Data bus pins: DB0=pin30, DB1=pin29, ..., DB5=pin23, DB4=pin26, ..., DB7=pin21
    Pin pin_db_[8];

    // Address pins (directly connected to bus for register select)
    Pin pin_a_[8];   // A0=pin32 .. A3=pin35, A4=pin37 .. A7=pin40

    // Control inputs
    Pin pin_ior_;    // Pin  1: ~IOR
    Pin pin_iow_;    // Pin  2: ~IOW
    Pin pin_cs_;     // Pin 11: ~CS
    Pin pin_clk_;    // Pin 12: CLK
    Pin pin_reset_;  // Pin 13: RESET
    Pin pin_ready_;  // Pin  6: READY
    Pin pin_hlda_;   // Pin  7: HLDA
    Pin pin_eop_;    // Pin 36: ~EOP
    Pin pin_vcc_;    // Pin  5: VCC

    // DREQ inputs
    Pin pin_dreq_[4];  // DREQ0=pin19, DREQ1=pin18, DREQ2=pin17, DREQ3=pin16

    // Outputs
    Pin pin_hrq_;      // Pin 10: HRQ
    Pin pin_dack_[4];  // DACK0=pin25, DACK1=pin24, DACK2=pin14, DACK3=pin15
    Pin pin_memr_;     // Pin  3: ~MEMR
    Pin pin_memw_;     // Pin  4: ~MEMW
    Pin pin_adstb_;    // Pin  8: ADSTB
    Pin pin_aen_;      // Pin  9: AEN

    // Channel state
    struct Channel {
        uint16_t base_address = 0;
        uint16_t base_count = 0;
        uint16_t current_address = 0;
        uint16_t current_count = 0;
        uint8_t mode = 0;           // mode register value
        bool masked = true;         // masked = DMA requests blocked
        bool request = false;       // software request pending
        bool tc_reached = false;    // terminal count flag (for status)
    };
    Channel ch_[4];

    // Internal registers
    uint8_t command_ = 0;       // command register
    uint8_t status_ = 0;        // status register (TC flags + DREQ status)
    uint8_t temp_ = 0;          // temporary register
    bool flip_flop_ = false;    // byte flip-flop (false=low byte, true=high byte)

    // DMA state machine -- matches datasheet Figure 11:
    //   SI: idle, polling DREQ
    //   BusRequested: HRQ asserted, waiting for HLDA
    //   S1-S4: active DMA transfer states
    enum class State { SI, BusRequested, S1, S2, S3, S4, M2M_S1, M2M_S2, M2M_S3, M2M_S4 };
    State state_ = State::SI;
    int active_ch_ = -1;        // which channel is currently active
    bool disabled_ = false;     // controller disabled (command bit 2)

    bool db_driving_ = false;   // true when we're actively driving data bus
    bool a_driving_ = false;    // true when we're driving address pins A0-A7
    bool write_pending_ = false; // deferred bus write (data not yet on bus)
    bool read_pending_ = false;  // deferred bus read
    bool eop_pending_ = false;   // EOP asserted this cycle, deassert next cycle
    IC_74S245* xcvr_ = nullptr;      // U8:  AD<->D transceiver
    IC_74S245* xcvr_m_ = nullptr;    // U12: D<->MD transceiver (DRAM)
    IC_74S245* xcvr_x_ = nullptr;    // U13: D<->XD transceiver (ISA)
    IC_74S245* xcvr_c_ = nullptr;    // U14: cmd strobe transceiver
    bool mem2mem_write_ = false; // true during write phase of mem-to-mem transfer
    uint8_t prev_upper_addr_ = 0; // last A8-A15 latched, for S1 skip optimization

    // Edge tracking
    Level reset_prev_ = Level::HiZ;
    Level iow_prev_ = Level::HiZ;
    Level cs_prev_ = Level::HiZ;
    Level ior_prev_ = Level::HiZ;
    Level clk_prev_ = Level::HiZ;
    Level hlda_prev_ = Level::HiZ;
};

} // namespace bench
