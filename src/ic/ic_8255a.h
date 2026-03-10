#pragma once
#include "core/component.h"
#include "board/socket.h"

namespace bench {

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
// Threading: Reactive IC. Uses default run() -- blocks on mailbox.
class IC_8255A : public Component {
public:
    IC_8255A();

    void install(Socket& socket);

protected:
    void on_signal_change() override;

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
    Signal* pin_d_[8] = {};

    // Port A pins (PA0=pin4, PA1=pin3, PA2=pin2, PA3=pin1, PA4=pin40, ..., PA7=pin37)
    Signal* pin_pa_[8] = {};

    // Port B pins (PB0=pin18 .. PB7=pin25)
    Signal* pin_pb_[8] = {};

    // Port C pins (PC0=pin14, PC1=pin15, PC2=pin16, PC3=pin17,
    //              PC4=pin13, PC5=pin12, PC6=pin11, PC7=pin10)
    Signal* pin_pc_[8] = {};

    // Control pins
    Signal* pin_cs_    = nullptr;  // Pin  6: ~CS
    Signal* pin_rd_    = nullptr;  // Pin  5: ~RD
    Signal* pin_wr_    = nullptr;  // Pin 36: ~WR
    Signal* pin_a0_    = nullptr;  // Pin  9: A0
    Signal* pin_a1_    = nullptr;  // Pin  8: A1
    Signal* pin_reset_ = nullptr;  // Pin 35: RESET
    Signal* pin_vcc_   = nullptr;  // Pin 26: VCC

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
};

} // namespace bench
