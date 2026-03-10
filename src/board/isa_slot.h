#pragma once
#include <string>
#include <memory>

namespace bench {

class Signal;
class Component;

// 8-bit ISA expansion slot (62 pins: A1-A31 component side, B1-B31 solder side).
// Directly maps to motherboard signals. A card (Component) can be inserted.
class IsaSlot {
public:
    explicit IsaSlot(std::string ref_designator);

    const std::string& ref() const { return ref_; }

    // Component side pins (directly accessible for wiring)
    // A1-A31
    Signal* io_ch_ck   = nullptr;  // A1  ~I/O CH CK
    Signal* sd[8]      = {};       // A2-A9: SD7..SD0
    Signal* io_ch_rdy  = nullptr;  // A10 I/O CH RDY
    Signal* aen        = nullptr;  // A11 AEN
    Signal* sa[20]     = {};       // A12-A31: SA19..SA0

    // Solder side pins
    // B1-B31
    Signal* gnd_b1     = nullptr;  // B1  GND
    Signal* reset_drv  = nullptr;  // B2  RESET DRV
    Signal* vcc_b3     = nullptr;  // B3  +5V
    Signal* irq2       = nullptr;  // B4  IRQ2
    Signal* vcc_n5     = nullptr;  // B5  -5V
    Signal* drq2       = nullptr;  // B6  DRQ2
    Signal* vcc_n12    = nullptr;  // B7  -12V
    Signal* reserved   = nullptr;  // B8  reserved
    Signal* vcc_12     = nullptr;  // B9  +12V
    Signal* gnd_b10    = nullptr;  // B10 GND
    Signal* memw       = nullptr;  // B11 ~MEMW
    Signal* memr       = nullptr;  // B12 ~MEMR
    Signal* iow        = nullptr;  // B13 ~IOW
    Signal* ior        = nullptr;  // B14 ~IOR
    Signal* dack3      = nullptr;  // B15 ~DACK3
    Signal* drq3       = nullptr;  // B16 DRQ3
    Signal* dack1      = nullptr;  // B17 ~DACK1
    Signal* drq1       = nullptr;  // B18 DRQ1
    Signal* dack0      = nullptr;  // B19 ~DACK0
    Signal* clk        = nullptr;  // B20 CLK
    Signal* irq7       = nullptr;  // B21 IRQ7
    Signal* irq6       = nullptr;  // B22 IRQ6
    Signal* irq5       = nullptr;  // B23 IRQ5
    Signal* irq4       = nullptr;  // B24 IRQ4
    Signal* irq3       = nullptr;  // B25 IRQ3
    Signal* dack2      = nullptr;  // B26 ~DACK2
    Signal* tc         = nullptr;  // B27 T/C (terminal count)
    Signal* ale        = nullptr;  // B28 ALE
    Signal* vcc_b29    = nullptr;  // B29 +5V
    Signal* osc        = nullptr;  // B30 OSC (14.31818 MHz)
    Signal* gnd_b31    = nullptr;  // B31 GND

    // Wire a pin by BRD pin number (1-62) to a signal.
    // Pins 1-31 = component side (A1-A31), pins 32-62 = solder side (B1-B31).
    void wire_pin(int pin, Signal* sig);

    // Insert/eject an expansion card.
    void insert(std::unique_ptr<Component> card);
    std::unique_ptr<Component> eject();
    bool occupied() const { return card_ != nullptr; }
    Component* card() const { return card_.get(); }

private:
    std::string ref_;
    std::unique_ptr<Component> card_;
};

} // namespace bench
