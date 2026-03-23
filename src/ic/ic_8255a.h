#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

class IC_8253;  // forward decl for speaker support

// Intel 8255A-5 Programmable Peripheral Interface.
//
// 40-pin DIP. Three 8-bit I/O ports (A, B, C) with programmable
// direction. Mode 0 only (the 5150 doesn't use modes 1 or 2).
//
// Pin functions (from BRD, U36 on 5150):
//   Pin  1-4:  PA3, PA2, PA1, PA0  (I/O)
//   Pin  5:    ~RD                  (input, read strobe = ~XIOR)
//   Pin  6:    ~CS                  (input, chip select = ~PPI_CS)
//   Pin  7:    GND
//   Pin  8:    A1                   (input, register select)
//   Pin  9:    A0                   (input, register select)
//   Pin 10-13: PC7, PC6, PC5, PC4  (I/O, upper nibble)
//   Pin 14-17: PC0, PC1, PC2, PC3  (I/O, lower nibble)
//   Pin 18-25: PB0-PB7             (I/O)
//   Pin 26:    VCC                  (+5V)
//   Pin 27-34: D7-D0               (bidirectional data bus)
//   Pin 35:    RESET                (input)
//   Pin 36:    ~WR                  (input, write strobe = ~XIOW)
//   Pin 37-40: PA7, PA6, PA5, PA4  (I/O)
//
// Register select (A1, A0):
//   00: Port A       01: Port B
//   10: Port C       11: Control register (write only)
//
// 5150 usage (control word 0x99):
//   Port A = input (SW1 switches or keyboard scancode)
//   Port B = output (speaker, cassette, keyboard control)
//   Port C upper = input (PCK, IO_CH_CK, T/C2, CASS_DATA_IN)
//   Port C lower = output (directly connected to SW2 on Model B)
//
// Callback IC -- never yields, completes all work in on_signal_change().
class IC_8255A : public CallbackComponent {
public:
    IC_8255A();

    void install(Socket& socket);

    // Speaker support: set PIT and CLK counter so we can Beep() on speaker off.
    void set_speaker_source(const IC_8253* pit, const uint64_t* clk_cycles) {
        pit_ = pit; clk_cycles_ = clk_cycles;
    }

protected:
    void on_signal_change(Fiber caller) override;

private:
    void on_bus_write();
    void on_bus_read();
    void on_reset();
    void drive_data(uint8_t value);
    void release_data();
    uint8_t read_data() const;
    uint8_t read_port_a() const;
    uint8_t read_port_b() const;
    uint8_t read_port_c() const;
    void write_port_a(uint8_t value);
    void write_port_b(uint8_t value);
    void write_port_c(uint8_t value);

    // Data bus pins D0-D7
    Pin pin_d_[8];

    // Port A pins (PA0=pin4, PA1=pin3, PA2=pin2, PA3=pin1, PA4=pin40, ..., PA7=pin37)
    Pin pin_pa_[8];

    // Port B pins (PB0=pin18 .. PB7=pin25)
    Pin pin_pb_[8];

    // Port C pins (PC0=pin14, PC1=pin15, PC2=pin16, PC3=pin17,
    //              PC4=pin13, PC5=pin12, PC6=pin11, PC7=pin10)
    Pin pin_pc_[8];

    // Control pins
    Pin pin_cs_;     // Pin  6: ~CS
    Pin pin_rd_;     // Pin  5: ~RD
    Pin pin_wr_;     // Pin 36: ~WR
    Pin pin_a0_;     // Pin  9: A0
    Pin pin_a1_;     // Pin  8: A1
    Pin pin_reset_;  // Pin 35: RESET
    Pin pin_vcc_;    // Pin 26: VCC

    // Internal state
    uint8_t control_ = 0x9B;  // default: all ports input
    uint8_t latch_a_ = 0;     // output latch for port A
    uint8_t latch_b_ = 0;     // output latch for port B
    uint8_t latch_c_ = 0;     // output latch for port C

    // Direction flags (derived from control word)
    bool pa_input_ = true;
    bool pb_input_ = true;
    bool pc_upper_input_ = true;
    bool pc_lower_input_ = true;

    // Edge tracking
    Level reset_prev_ = Level::HiZ;
    Level wr_prev_ = Level::HiZ;
    Level cs_prev_ = Level::HiZ;
    Level rd_prev_ = Level::HiZ;
    bool write_pending_ = false;   // deferred write: data settles one cycle after ~WR falls

    // Speaker support
    const IC_8253* pit_ = nullptr;
    const uint64_t* clk_cycles_ = nullptr;
    uint64_t speaker_on_clk_ = 0;
};

} // namespace bench
