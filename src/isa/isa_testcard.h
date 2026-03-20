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
//   - DMA channels 1-3 support:
//     Port 0xF4: write channel (1-3) = assert DRQn (start DMA transfer)
//     Port 0xF5: write channel (1-3) = deassert DRQn
//     Port 0xF6: write IRQ number (2-7) for DMA completion notification
//     The card drives sequential bytes from dma_buf_ on each ~DACKn pulse.
//   - MMIO: 16KB at 0xB8000-0xBBFFF (CGA-style video RAM region)
//     Responds to ~MEMR/~MEMW when address is in range.
//
// Reads ISA bus signals (SA0-SA19, SD0-SD7, ~IOR, ~IOW, ~MEMR, ~MEMW)
// directly from the slot connector.
class ISA_TestCard : public CallbackComponent {
public:
    ISA_TestCard();

    void install(IsaSlot& slot);

    // Public accessors for test harness.
    uint8_t* io_data() { return io_.get(); }

    // DMA buffer: test harness preloads data here before starting DMA.
    static constexpr int DMA_BUF_SIZE = 256;
    uint8_t* dma_buf() { return dma_buf_; }

    // MMIO: 16KB at 0xB8000-0xBBFFF (test harness can preload).
    uint8_t* mmio_data() { return mmio_; }
    static constexpr uint32_t MMIO_BASE = 0xB8000;
    static constexpr uint32_t MMIO_SIZE = 16 * 1024;

protected:
    void on_power_on() override;
    void on_signal_change(Fiber caller) override;

private:
    // ISA bus pins
    Pin sd_[8];       // SD0-SD7 (data bus, bidirectional)
    Pin sa_[20];      // SA0-SA19 (address bus, input)
    Pin ior_;         // ~IOR (input, active-low read strobe)
    Pin iow_;         // ~IOW (input, active-low write strobe)
    Pin memr_;        // ~MEMR (input, active-low memory read)
    Pin memw_;        // ~MEMW (input, active-low memory write)

    // IRQ output pins (test trigger/clear)
    Pin irq_pin_[8];  // IRQ0-IRQ7 (output)
    Signal* irq_sig_[8] = {};

    // DMA channels 1-3: ~DACKn (input), DRQn (output)
    Pin dack_[4];         // ~DACK0-3 (input, only 1-3 used)
    Pin drq_[4];          // DRQ0-3 (output, only 1-3 used)
    Signal* drq_sig_[4] = {};
    Level dack_prev_[4] = {Level::HiZ, Level::HiZ, Level::HiZ, Level::HiZ};
    int dma_ior_count_ = 0;       // IOR falling edges since last DACK fall

    // T/C (terminal count) from ISA bus -- fires when DMA transfer completes.
    Pin tc_;          // T/C (input, active-high terminal count pulse)

    // I/O port space
    std::unique_ptr<uint8_t[]> io_ = std::make_unique<uint8_t[]>(1 << 16);

    // DMA transfer buffer and state (shared across channels)
    uint8_t dma_buf_[DMA_BUF_SIZE] = {};
    uint16_t dma_ptr_ = 0;        // current offset into dma_buf_
    int dma_active_ch_ = -1;      // which channel is active (-1 = none)
    uint8_t dma_irq_ = 5;         // IRQ to fire on DMA completion (default IRQ5)
    Level tc_prev_ = Level::HiZ;

    // Edge tracking
    Level ior_prev_ = Level::HiZ;
    Level iow_prev_ = Level::HiZ;
    Level memr_prev_ = Level::HiZ;
    Level memw_prev_ = Level::HiZ;
    bool mem_write_pending_ = false;
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

    uint8_t mmio_[MMIO_SIZE] = {};
    static bool my_mmio(uint32_t addr) { return addr >= MMIO_BASE && addr < MMIO_BASE + MMIO_SIZE; }

    // I/O handlers
    uint8_t io_read(uint16_t port);
    void    io_write(uint16_t port, uint8_t val);

    // Helper: is any DMA channel active?
    bool dma_active() const { return dma_active_ch_ >= 0; }
};

} // namespace bench
