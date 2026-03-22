#pragma once
#include "core/callback_component.h"
#include "board/isa_slot.h"

namespace bench {

// ISA_Adapter -- base class for 8-bit ISA expansion cards.
//
// Manages all low-level bus connectivity (the "edge connector"):
//   - SD0-SD7 data bus (bidirectional, with deferred read/write pattern)
//   - SA0-SA19 address bus (input)
//   - ~IOR, ~IOW, ~MEMR, ~MEMW control strobes (edge-tracked)
//   - DMA channels 1-3: ~DACKn (input), DRQn (output), T/C (input)
//   - IRQ2-IRQ7 outputs
//
// Subclasses implement virtual methods to claim specific port/address ranges
// and handle the actual I/O, MMIO, and DMA data.
//
// Future ISA cards derive from this:
//   ISA_FloppyController: claims_port(0x3F0-0x3F7), DMA ch2, IRQ 6
//   ISA_MDA: claims_mmio(0xB0000-0xB0FFF), claims_port(0x3B0-0x3BF)
//   ISA_CGA: claims_mmio(0xB8000-0xBFFFF), claims_port(0x3D0-0x3DF)
//
// IMPORTANT: install() must be called after the derived class is fully
// constructed (not from a base constructor), because the bidir lambda
// captures `this` and calls virtual methods at runtime.
class ISA_Adapter : public CallbackComponent {
public:
    explicit ISA_Adapter(std::string name, uint8_t dma_channels = 0x0E);

    void install(IsaSlot& slot);

    // Reconfigure which DMA channels this card owns (bitmask, bits 1-3).
    // Must be called BEFORE install(). Channels not in the mask are not
    // wired and won't generate DAG edges -- allowing another card in a
    // different slot to own them.
    void set_dma_channels(uint8_t mask) { dma_channel_mask_ = mask; }
    uint8_t dma_channels() const { return dma_channel_mask_; }

protected:
    // --- Subclass contract (pure virtual) ---

    // Port/address claiming -- called from bidir lambda and on_signal_change.
    virtual bool claims_port(uint16_t port) = 0;
    virtual bool claims_mmio(uint32_t addr) = 0;

    // I/O port handlers (called after deferred read/write settles).
    virtual uint8_t on_io_read(uint16_t port) = 0;
    virtual void    on_io_write(uint16_t port, uint8_t val) = 0;

    // MMIO handlers.
    virtual uint8_t on_mmio_read(uint32_t addr) = 0;
    virtual void    on_mmio_write(uint32_t addr, uint8_t val) = 0;

    // DMA: provide next byte for transfer. Subclass manages its own pointer.
    virtual uint8_t on_dma_read() = 0;

    // DMA: terminal count fired for this channel.
    virtual void on_dma_complete(int channel) = 0;

    // --- Protected helpers for subclasses ---

    void raise_irq(int n);
    void lower_irq(int n);
    void assert_drq(int ch);
    void deassert_drq(int ch);

    // Bus read helpers (available to subclasses for advanced use).
    uint32_t read_address();
    uint8_t  read_sd();
    void     drive_sd(uint8_t val);
    void     release_sd();
    bool     dma_active() const { return dma_active_ch_ >= 0; }
    int      dma_active_channel() const { return dma_active_ch_; }

    // Lifecycle -- subclass must call ISA_Adapter::on_power_on() first.
    void on_power_on() override;
    void on_signal_change(Fiber caller) override;

private:
    // ISA bus pins
    Pin sd_[8]{};       // SD0-SD7 (data bus, bidirectional)
    Pin sa_[20]{};      // SA0-SA19 (address bus, input)
    Pin ior_{};         // ~IOR (input)
    Pin iow_{};         // ~IOW (input)
    Pin memr_{};        // ~MEMR (input)
    Pin memw_{};        // ~MEMW (input)

    // IRQ output pins (directly driven by raise_irq/lower_irq)
    Pin irq_pin_[8]{};
    Signal* irq_sig_[8] = {};

    // DMA channels 1-3
    Pin dack_[4]{};
    Pin drq_[4]{};
    Signal* drq_sig_[4] = {};
    Level dack_prev_[4] = {Level::HiZ, Level::HiZ, Level::HiZ, Level::HiZ};
    bool dma_dack_pending_ = false;
    int dma_ior_count_ = 0;

    // Terminal count
    Pin tc_{};
    Level tc_prev_ = Level::HiZ;

    // DMA state
    int dma_active_ch_ = -1;
    uint8_t dma_channel_mask_ = 0x0E;  // default: channels 1-3
    bool owns_dma(int ch) const { return (dma_channel_mask_ & (1 << ch)) != 0; }

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
};

} // namespace bench
