#pragma once
#include "core/callback_component.h"
#include "board/socket.h"
#include <array>
#include <string>

namespace bench {

// Unified 40KB ROM: U29-U33 on the IBM PC 5150 motherboard.
// 5 banks x 8KB = 40KB. U29-U32 = Cassette BASIC, U33 = BIOS.
//
// All banks share address (A0-A12) and data (D0-D7) pins (same nets).
// Each bank has its own ~CS from the U46 74S138 decoder.
// Only one ~CS is active at a time.
//
// Pinout per socket (2364/2764-compatible, 24-pin DIP):
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
// Address mapping (via U46 decoder):
//   Bank 0 (U29): F6000-F7FFF  ~Y3
//   Bank 1 (U30): F8000-F9FFF  ~Y4
//   Bank 2 (U31): FA000-FBFFF  ~Y5
//   Bank 3 (U32): FC000-FDFFF  ~Y6
//   Bank 4 (U33): FE000-FFFFF  ~Y7
class IC_ROM_40K : public CallbackComponent {
public:
    IC_ROM_40K();

    // Load an 8KB ROM image into a bank (0=U29 .. 4=U33).
    void load(int bank, const std::string& file_path);

    // Install across all 5 ROM sockets.
    void install(Socket& u29, Socket& u30, Socket& u31, Socket& u32, Socket& u33);

    // Raw access for test harness inspection (power-off only).
    const uint8_t* bank_data(int bank) const { return rom_.data() + bank * 8192; }
    static constexpr size_t bank_size() { return 8192; }
    static constexpr size_t total_size() { return 5 * 8192; }

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_signal_change(Fiber caller) override;

private:
    uint16_t read_address() const;

    std::array<uint8_t, 5 * 8192> rom_{};  // 40KB: 5 banks x 8KB

    Pin pin_a_[13];   // A0-A12 (shared, from first socket)
    Pin pin_d_[8];    // D0-D7 (shared, from first socket)
    Pin pin_vcc_;     // VCC

    struct Bank {
        Pin cs;       // ~CS from U46 decoder
    };
    Bank banks_[5];   // 0=U29, 1=U30, 2=U31, 3=U32, 4=U33

    int active_bank_ = -1;  // which bank is currently driving (-1 = none)
};

} // namespace bench
