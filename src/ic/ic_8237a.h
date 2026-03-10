#pragma once
#include "core/component.h"
#include "board/socket.h"

namespace bench {

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
// 5150 usage:
//   CH0: DRAM refresh (auto-init, single transfer, DREQ0 from PIT ch1)
//   CH1-CH3: available for ISA peripherals
//
// Threading: Reactive IC. Uses default run() -- blocks on mailbox.
class IC_8237A : public Component {
public:
    IC_8237A();

    void install(Socket& socket);

protected:
    void on_signal_change() override;

private:
    void on_bus_write();
    void on_bus_read();
    void on_reset();
    void on_clk_falling();
    void evaluate_dreq();
    void drive_data(uint8_t value);
    void release_data();
    uint8_t read_data() const;

    // Data bus pins: DB0=pin30, DB1=pin29, ..., DB5=pin23, DB4=pin26, ..., DB7=pin21
    Signal* pin_db_[8] = {};

    // Address pins (directly connected to bus for register select)
    Signal* pin_a_[8] = {};   // A0=pin32 .. A3=pin35, A4=pin37 .. A7=pin40

    // Control inputs
    Signal* pin_ior_   = nullptr;  // Pin  1: ~IOR
    Signal* pin_iow_   = nullptr;  // Pin  2: ~IOW
    Signal* pin_cs_    = nullptr;  // Pin 11: ~CS
    Signal* pin_clk_   = nullptr;  // Pin 12: CLK
    Signal* pin_reset_ = nullptr;  // Pin 13: RESET
    Signal* pin_ready_ = nullptr;  // Pin  6: READY
    Signal* pin_hlda_  = nullptr;  // Pin  7: HLDA
    Signal* pin_eop_   = nullptr;  // Pin 36: ~EOP
    Signal* pin_vcc_   = nullptr;  // Pin  5: VCC

    // DREQ inputs
    Signal* pin_dreq_[4] = {};  // DREQ0=pin19, DREQ1=pin18, DREQ2=pin17, DREQ3=pin16

    // Outputs
    Signal* pin_hrq_   = nullptr;  // Pin 10: HRQ
    Signal* pin_dack_[4] = {};     // DACK0=pin25, DACK1=pin24, DACK2=pin14, DACK3=pin15
    Signal* pin_memr_  = nullptr;  // Pin  3: ~MEMR
    Signal* pin_memw_  = nullptr;  // Pin  4: ~MEMW
    Signal* pin_adstb_ = nullptr;  // Pin  8: ADSTB
    Signal* pin_aen_   = nullptr;  // Pin  9: AEN

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

    // DMA state machine
    enum class State { Idle, RequestPending, Transfer };
    State state_ = State::Idle;
    int active_ch_ = -1;        // which channel is currently active
    bool disabled_ = false;     // controller disabled (command bit 2)

    // Edge tracking
    Level reset_prev_ = Level::HiZ;
    Level iow_prev_ = Level::HiZ;
    Level cs_prev_ = Level::HiZ;
    Level ior_prev_ = Level::HiZ;
    Level clk_prev_ = Level::HiZ;
    Level hlda_prev_ = Level::HiZ;
};

} // namespace bench
