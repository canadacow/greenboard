#pragma once
#include "core/fiber_component.h"
#include "board/socket.h"

namespace bench {

// Intel 8253-5 Programmable Interval Timer.
//
// 24-pin DIP. Three independent 16-bit down-counters, each with CLK,
// GATE, and OUT pins. Programmed via data bus using ~CS, ~RD, ~WR.
//
// Pin functions (from BRD/datasheet):
//   Pin  1: D7       (data bus)
//   Pin  2: D6
//   Pin  3: D5
//   Pin  4: D4
//   Pin  5: D3
//   Pin  6: D2
//   Pin  7: D1
//   Pin  8: D0
//   Pin  9: CLK0     (input, counter 0 clock -- 1.193182 MHz on 5150)
//   Pin 10: OUT0     (output, counter 0 output -- IRQ0 timer interrupt)
//   Pin 11: GATE0    (input, counter 0 gate -- +5V, always enabled)
//   Pin 12: GND
//   Pin 13: OUT1     (output, counter 1 output -- DMA refresh request)
//   Pin 14: GATE1    (input, counter 1 gate -- +5V, always enabled)
//   Pin 15: CLK1     (input, counter 1 clock -- same 1.193 MHz)
//   Pin 16: GATE2    (input, counter 2 gate -- PPI PB0, speaker enable)
//   Pin 17: OUT2     (output, counter 2 output -- speaker/cassette)
//   Pin 18: CLK2     (input, counter 2 clock -- same 1.193 MHz)
//   Pin 19: A0       (input, address bit 0 -- counter select)
//   Pin 20: A1       (input, address bit 1 -- counter select)
//   Pin 21: ~CS      (input, chip select -- active low)
//   Pin 22: ~RD      (input, read strobe -- active low)
//   Pin 23: ~WR      (input, write strobe -- active low)
//   Pin 24: VCC      (+5V)
//
// Counter modes:
//   Mode 0: Interrupt on terminal count
//   Mode 1: Hardware retriggerable one-shot
//   Mode 2: Rate generator (periodic pulse)
//   Mode 3: Square wave generator
//   Mode 4: Software triggered strobe
//   Mode 5: Hardware triggered strobe
//
// Control word format (written to address A1=1, A0=1):
//   Bits 7-6: Counter select (00=Ch0, 01=Ch1, 10=Ch2, 11=read-back)
//   Bits 5-4: R/W mode (00=latch, 01=LSB, 10=MSB, 11=LSB then MSB)
//   Bits 3-1: Mode (000..101)
//   Bit    0: BCD (0=binary 16-bit, 1=BCD 4-decade)
//
// IBM PC 5150 BIOS programs:
//   Ch0: Mode 3, count 0 (65536) -> 18.2 Hz system timer (IRQ0)
//   Ch1: Mode 2, count 18       -> ~66 kHz DRAM refresh DMA requests
//   Ch2: Mode 3, count varies   -> speaker tone frequency
//
// Threading: Reactive IC. Uses default run() -- blocks on mailbox,
// dispatches on_signal_change() for CLK falling edges and bus operations.
class IC_8253 : public FiberComponent {
public:
    IC_8253();

    void install(Socket& socket);

protected:
    void on_signal_change() override;

private:
    // A single counter channel.
    struct Channel {
        // Programming state
        uint8_t mode = 0;          // Operating mode (0-5)
        bool bcd = false;          // BCD counting (not used on 5150)
        uint8_t rw_mode = 0;       // 0=latch, 1=LSB, 2=MSB, 3=LSB+MSB
        bool programmed = false;   // Has control word been written?

        // Count registers
        uint16_t count = 0;        // Current counting element (CE)
        uint16_t reload = 0;       // Count register (CR) -- reload value
        uint16_t latch = 0;        // Output latch (for latched reads)
        bool latched = false;      // Latch is loaded, reads come from latch

        // Load state machine (for 2-byte loads)
        bool load_lsb_pending = false;  // LSB written, waiting for MSB
        uint8_t load_lsb_value = 0;     // Stored LSB during 2-byte write

        // Read state machine (for 2-byte reads)
        bool read_msb_next = false;     // Next read returns MSB

        // Output and gate state
        bool out = true;           // Current OUT pin level (starts high)
        bool gate = true;          // Current GATE input level
        bool counting = false;     // Counter is actively counting
        bool loaded = false;       // Count register has been loaded at least once
        bool null_count = true;    // CR loaded but not yet transferred to CE
    };

    // Counter operations
    void on_clk_falling(int ch);
    void on_gate_change(int ch, bool new_gate);
    void write_control(uint8_t value);
    void write_counter(int ch, uint8_t value);
    uint8_t read_counter(int ch);
    void update_out(int ch);
    uint16_t decrement(uint16_t val, bool bcd);

    // Bus interface
    void on_write_falling();
    void on_read_falling();

    Channel channels_[3];

    // Output pins (we drive these)
    Signal* pin_out_[3] = {};     // Pins 10, 13, 17: OUT0, OUT1, OUT2

    // Input pins
    Signal* pin_clk_[3] = {};     // Pins 9, 15, 18: CLK0, CLK1, CLK2
    Signal* pin_gate_[3] = {};    // Pins 11, 14, 16: GATE0, GATE1, GATE2
    Signal* pin_data_[8] = {};    // Pins 8-1: D0-D7
    Signal* pin_a0_ = nullptr;    // Pin 19: A0
    Signal* pin_a1_ = nullptr;    // Pin 20: A1
    Signal* pin_cs_ = nullptr;    // Pin 21: ~CS
    Signal* pin_rd_ = nullptr;    // Pin 22: ~RD
    Signal* pin_wr_ = nullptr;    // Pin 23: ~WR
    Signal* pin_vcc_ = nullptr;   // Pin 24: VCC

    // Edge tracking
    Level clk_prev_[3] = {Level::HiZ, Level::HiZ, Level::HiZ};
    Level gate_prev_[3] = {Level::HiZ, Level::HiZ, Level::HiZ};
    Level wr_prev_ = Level::HiZ;
    Level rd_prev_ = Level::HiZ;
};

} // namespace bench
