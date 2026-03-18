#pragma once
#include "core/callback_component.h"
#include "board/isa_slot.h"
#include <cstring>

namespace bench {

// ISA Test Card -- plugs into an 8-bit ISA expansion slot.
//
// Provides:
//   - I/O port space for ports > 0x7F (non hardware-decoded range)
//   - Test trigger port 0xF0: write bit N to raise IRQ N
//   - Test clear port 0xF1: write bit N to lower IRQ N
//   - DMA channel 1 support:
//     Port 0xF2: write = load DMA buffer pointer offset (low byte)
//     Port 0xF3: write = load DMA buffer pointer offset (high byte)
//     Port 0xF4: write = assert DRQ1 (start DMA transfer)
//     Port 0xF5: write = deassert DRQ1
//     The card drives sequential bytes from dma_buf_ on each ~DACK1 pulse.
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

    // DMA buffer: test harness preloads data here before starting DMA.
    static constexpr int DMA_BUF_SIZE = 256;
    uint8_t* dma_buf() { return dma_buf_; }

protected:
    void on_power_on() override;
    void on_signal_change(Fiber caller) override;

private:
    // ISA bus pins
    Pin sd_[8];       // SD0-SD7 (data bus, bidirectional)
    Pin sa_[20];      // SA0-SA19 (address bus, input)
    Pin ior_;         // ~IOR (input, active-low read strobe)
    Pin iow_;         // ~IOW (input, active-low write strobe)

    // IRQ output pins (test trigger/clear)
    Pin irq_pin_[8];  // IRQ0-IRQ7 (output)
    Signal* irq_sig_[8] = {};

    // DMA channel 1 pins
    Pin dack1_;       // ~DACK1 (input, active-low DMA acknowledge)
    Pin drq1_;        // DRQ1 (output, DMA request)
    Signal* drq1_sig_ = nullptr;

    // T/C (terminal count) from ISA bus -- fires when DMA transfer completes.
    Pin tc_;          // T/C (input, active-high terminal count pulse)

    // I/O port space
    std::unique_ptr<uint8_t[]> io_ = std::make_unique<uint8_t[]>(1 << 16);

    // DMA transfer buffer and state
    uint8_t dma_buf_[DMA_BUF_SIZE] = {};
    uint16_t dma_ptr_ = 0;        // current offset into dma_buf_
    bool dma_active_ = false;     // DRQ1 asserted
    uint8_t dma_irq_ = 5;        // IRQ to fire on DMA completion (default IRQ5)
    Level dack1_prev_ = Level::HiZ;
    Level tc_prev_ = Level::HiZ;

    // Edge tracking
    Level ior_prev_ = Level::HiZ;
    Level iow_prev_ = Level::HiZ;
    bool data_driven_ = false;
    uint8_t read_byte_ = 0;
    bool write_pending_ = false;
    bool read_pending_ = false;

    // Bus helpers
    uint32_t read_address();
    uint8_t  read_sd();
    void     drive_sd(uint8_t val);
    void     release_sd();

    // Address decode: this card claims ports 0x80-0xFF.
    static bool my_port(uint16_t port) { return (port & 0xFF80) == 0x0080; }

    // I/O handlers
    uint8_t io_read(uint16_t port);
    void    io_write(uint16_t port, uint8_t val);
};

} // namespace bench
