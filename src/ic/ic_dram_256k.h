#pragma once
#include "core/callback_component.h"
#include "core/signal.h"
#include "board/socket.h"
#include <array>
#include <vector>

namespace bench {

// All four banks of 4164 64Kx1 DRAM on the IBM PC 5150 motherboard,
// represented as a single behavioral component.
//
// 4 banks x 9 chips (8 data + 1 parity) = 36 physical chips.
// Total capacity: 256KB + parity.
//
// Shared across all banks: MA0-MA7 (multiplexed address), ~WE.
// Per-bank: ~RAS, ~CAS, 9 data lines (DIN/DOUT per chip).
//
// 4164 pinout (16-pin DIP):
//   1=NC  2=DIN  3=~WE  4=~RAS  5=A0  6=A2  7=A1  8=VCC
//   9=A7  10=A5  11=A4  12=A3  13=A6  14=DOUT  15=~CAS  16=GND
class IC_DRAM_256K : public CallbackComponent {
public:
    IC_DRAM_256K();

    // Install across all 4 banks of 9 sockets each.
    // Each bank: socket[0]=parity, socket[1..8]=MD0..MD7.
    void install(std::vector<Socket>& bank0, std::vector<Socket>& bank1,
                 std::vector<Socket>& bank2, std::vector<Socket>& bank3);

    // Raw access for test harness pre-load / readback (power-off only).
    // All runtime memory access goes through the pin-level RAS/CAS protocol.
    uint8_t* data() { return ram_.data(); }
    const uint8_t* data() const { return ram_.data(); }
    static constexpr size_t size() { return 256 * 1024; }

protected:
    void on_power_on() override;
    void on_power_off() override;
    void on_cycle(Fiber caller) override;

private:
    uint8_t read_address() const;

    // 256KB data + parity
    std::array<uint8_t, 256 * 1024> ram_{};
    std::array<uint8_t, 256 * 1024> parity_{};  // 1 bit per byte

    // Address pins (shared -- same net for all 36 chips)
    Pin pin_a_[8];    // A0..A7

    // Shared control
    Pin pin_we_;      // ~WE
    Pin pin_vcc_;     // VCC

    // Per-bank state
    struct Bank {
        Pin ras;          // ~RAS
        Pin cas;          // ~CAS
        Pin din[9];       // DIN per chip (0..7=data, 8=parity)
        Pin dout[9];      // DOUT per chip

        uint8_t row_addr = 0;
        bool row_latched = false;
        bool driving = false;
        Level ras_prev = Level::HiZ;
        Level cas_prev = Level::HiZ;
    };
    Bank banks_[4];
    int active_bank_ = -1;  // which bank has ~RAS Low (-1 = none)
};

} // namespace bench
