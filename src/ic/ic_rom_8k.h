#pragma once
#include "core/component.h"
#include "board/socket.h"
#include <array>
#include <string>

namespace bench {

// 8K x 8 Mask ROM (2364 / 2764-compatible).
//
// 24-pin DIP. U28-U33 on the 5150 motherboard.
//
// Pin functions (from BRD, 2364 pinout):
//   Pin  1: A7          Pin 24: VCC (+5V)
//   Pin  2: A6          Pin 23: A8
//   Pin  3: A5          Pin 22: A9
//   Pin  4: A4          Pin 21: A12
//   Pin  5: A3          Pin 20: ~CS (chip select, active low)
//   Pin  6: A2          Pin 19: A10
//   Pin  7: A1          Pin 18: A11
//   Pin  8: A0          Pin 17: D7
//   Pin  9: D0          Pin 16: D6
//   Pin 10: D1          Pin 15: D5
//   Pin 11: D2          Pin 14: D4
//   Pin 12: GND         Pin 13: D3
//
// Behavior: When ~CS is Low, drives rom[address] onto D0-D7.
//           When ~CS is High, D0-D7 are tri-stated.
//
// Threading: Reactive IC. Default run() -- blocks on mailbox.
class IC_ROM_8K : public Component {
public:
    explicit IC_ROM_8K(const std::string& label = "ROM");

    bool load(const std::string& file_path);
    void install(Socket& socket);

protected:
    void on_signal_change(Signal& signal, Level old_level, Level new_level) override;

private:
    void drive_output();
    void release_output();
    uint16_t read_address() const;

    std::array<uint8_t, 8192> rom_{};

    Signal* pin_a_[13] = {};  // A0(pin8)..A12(pin21)
    Signal* pin_d_[8]  = {};  // D0(pin9)..D7(pin17)
    Signal* pin_cs_    = nullptr;  // Pin 20: ~CS
    Signal* pin_vcc_   = nullptr;  // Pin 24: VCC
};

} // namespace bench
