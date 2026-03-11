#pragma once
#include "core/threaded_component.h"
#include "board/socket.h"

namespace bench {

// Intel 8259A Programmable Interrupt Controller.
//
// 28-pin DIP. Manages 8 prioritized interrupt request lines (IR0-IR7)
// and interfaces with the CPU via INT and ~INTA.
//
// Pin functions (from BRD, U2 on 5150):
//   Pin  1: ~CS     (input, chip select from address decode)
//   Pin  2: ~WR     (input, write strobe)
//   Pin  3: ~RD     (input, read strobe)
//   Pin  4-11: D7-D0 (bidirectional data bus)
//   Pin 12-13: CAS0-1 (cascade, NC on 5150 -- single PIC)
//   Pin 14: GND
//   Pin 15: CAS2    (cascade, NC on 5150)
//   Pin 16: ~SP/~EN (slave program / enable buffer)
//   Pin 17: INT     (output, interrupt request to CPU)
//   Pin 18-25: IR0-IR7 (input, interrupt request lines)
//   Pin 26: ~INTA   (input, interrupt acknowledge from 8288)
//   Pin 27: A0      (input, register select)
//   Pin 28: VCC     (+5V)
//
// 5150 configuration: edge-triggered, single PIC (no cascade), master mode.
// BIOS init: ICW1=0x13, ICW2=0x08 (IRQ0=INT 08h), ICW4=0x09 (8086 mode).
//
// Threading: Reactive IC. Uses default run() -- blocks on mailbox.
class IC_8259A : public ThreadedComponent {
public:
    IC_8259A();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_signal_change() override;

private:
    // Initialization state machine
    enum class InitState { Ready, WaitICW2, WaitICW3, WaitICW4 };

    void on_bus_write();
    void on_bus_read();
    void on_inta_falling();
    void evaluate_int();
    void drive_data(uint8_t value);
    void release_data();
    uint8_t read_data() const;
    int highest_priority_irq(uint8_t reg) const;

    // Data bus pins (D0-D7, active low pin numbers 11..4)
    Signal* pin_d_[8] = {};   // D0=pin11, D1=pin10, ..., D7=pin4

    // Input pins
    Signal* pin_cs_   = nullptr;  // Pin  1: ~CS
    Signal* pin_wr_   = nullptr;  // Pin  2: ~WR
    Signal* pin_rd_   = nullptr;  // Pin  3: ~RD
    Signal* pin_spen_ = nullptr;  // Pin 16: ~SP/~EN
    Signal* pin_ir_[8] = {};      // Pin 18-25: IR0-IR7
    Signal* pin_inta_ = nullptr;  // Pin 26: ~INTA
    Signal* pin_a0_   = nullptr;  // Pin 27: A0
    Signal* pin_vcc_  = nullptr;  // Pin 28: VCC

    // Output pins
    Signal* pin_int_  = nullptr;  // Pin 17: INT

    // Internal registers
    uint8_t irr_ = 0;    // Interrupt Request Register
    uint8_t isr_ = 0;    // In-Service Register
    uint8_t imr_ = 0;    // Interrupt Mask Register (OCW1)
    uint8_t vector_base_ = 0;  // ICW2: base interrupt vector (upper 5 bits)

    // ICW state
    uint8_t icw1_ = 0;
    bool icw4_needed_ = false;
    bool single_mode_ = true;  // true = no cascade (5150)
    bool edge_triggered_ = true;

    // ICW4 options
    bool auto_eoi_ = false;
    bool mode_8086_ = true;

    // Edge detection: previous IR pin levels (for edge-triggered mode)
    uint8_t ir_prev_ = 0;

    // OCW3 state: what to read on A0=0 read
    bool read_isr_ = false;  // false=IRR, true=ISR

    // INTA state: track first/second pulse
    int inta_count_ = 0;
    int inta_level_ = -1;  // which IRQ is being acknowledged

    // Initialization state
    InitState init_state_ = InitState::Ready;
    bool initialized_ = false;

    // Edge tracking for on_signal_change
    Level wr_prev_ = Level::HiZ;
    Level cs_prev_ = Level::HiZ;
    Level rd_prev_ = Level::HiZ;
    Level inta_prev_ = Level::HiZ;
};

} // namespace bench
