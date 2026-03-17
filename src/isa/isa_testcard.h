#pragma once
#include "core/callback_component.h"
#include "board/isa_slot.h"

namespace bench {

// ISA Test Card -- plugs into an 8-bit ISA expansion slot.
//
// Provides:
//   - I/O port space for ports > 0x7F (non hardware-decoded range)
//   - Test trigger port 0xF0: write bit N to raise IRQ N
//   - Test clear port 0xF1: write bit N to lower IRQ N
//
// Reads ISA bus signals (SA0-SA19, SD0-SD7, ~IOR, ~IOW) directly from
// the slot connector. Reacts to ~IOR/~IOW edges to drive/read the data
// bus -- no T-state machine needed since the 8288 has already decoded
// the bus cycle type.
class ISA_TestCard : public CallbackComponent {
public:
    ISA_TestCard();

    void install(IsaSlot& slot);

    // Public accessors for test harness.
    uint8_t* io_data() { return io_.get(); }

    void reset_state();

protected:
    void on_signal_change(Fiber caller) override;

private:
    // ISA bus pins
    Pin sd_[8];       // SD0-SD7 (data bus, bidirectional)
    Pin sa_[20];      // SA0-SA19 (address bus, input)
    Pin ior_;         // ~IOR (input, active-low read strobe)
    Pin iow_;         // ~IOW (input, active-low write strobe)

    // IRQ output pins (accent accent accent accent accent accent test trigger/clear)
    Pin irq_pin_[8];  // IRQ0-IRQ7 (accent output)
    Signal* irq_sig_[8] = {};

    // I/O port space
    std::unique_ptr<uint8_t[]> io_ = std::make_unique<uint8_t[]>(1 << 16);

    // Edge tracking
    Level ior_prev_ = Level::HiZ;
    Level iow_prev_ = Level::HiZ;
    bool data_driven_ = false;
    uint8_t read_byte_ = 0;

    // Bus helpers
    uint32_t read_address();
    uint8_t  read_sd();
    void     drive_sd(uint8_t val);
    void     release_sd();

    // I/O decode: ports 0x00-0x7F are handled by real ICs on the motherboard.
    static bool is_hw_decoded(uint16_t port) { return port <= 0x7F; }

    // I/O handlers
    uint8_t io_read(uint16_t port);
    void    io_write(uint16_t port, uint8_t val);
};

} // namespace bench
