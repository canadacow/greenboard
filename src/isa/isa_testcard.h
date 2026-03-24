#pragma once
#include "isa/isa_card.h"
#include "isa/isa_bus.h"
#include "test/test_keyboard.h"
#include <cstring>
#include <memory>

namespace bench {

// ISA Test Card -- plugs into an 8-bit ISA expansion slot.
//
// Provides:
//   - I/O port space for ports 0x80-0xFF (non hardware-decoded range)
//   - Test trigger port 0xF0: write bit N to raise IRQ N
//   - Test clear port 0xF1: write bit N to lower IRQ N
//   - DMA channels 1-3 support:
//     Port 0xF4: write channel (1-3) = assert DRQn (start DMA transfer)
//     Port 0xF5: write channel (1-3) = deassert DRQn
//     Port 0xF6: write IRQ number (2-7) for DMA completion notification
//     The card drives sequential bytes from dma_buf_ on each ~DACKn pulse.
class ISA_TestCard final : public ISA_Card {
public:
    ISA_TestCard();

    const std::string& card_name() const override { return name_; }

    // Public accessors for test harness.
    uint8_t* io_data() { return io_.get(); }

    // Keyboard ready signal: driven High when test program writes to port 0xFC.
    void set_kbd_ready_signal(Signal* sig) { kbd_ready_ = sig; }

    // Keyboard ACK signal: pulsed when IRQ handler writes scancode to port 0xFD.
    void set_kbd_ack_signal(Signal* sig) { kbd_ack_ = sig; }

    // Keyboard component: port 0xFB enqueues a scancode, port 0xFA enqueues an ASCII string.
    void set_keyboard(TestKeyboard* kbd) { keyboard_ = kbd; }

    // DMA buffer: test harness preloads data here before starting DMA.
    static constexpr int DMA_BUF_SIZE = 256;
    uint8_t* dma_buf() { return dma_buf_; }

    // ISA_Card overrides
    void on_power_on() override;
    bool claims_port(uint16_t port) override;
    bool claims_mmio(uint32_t addr) override { return false; }
    uint8_t on_io_read(uint16_t port) override;
    void    on_io_write(uint16_t port, uint8_t val) override;
    uint8_t on_mmio_read(uint32_t addr) override { return 0xFF; }
    void    on_mmio_write(uint32_t addr, uint8_t val) override {}
    uint8_t on_dma_read() override;
    void    on_dma_complete(int channel) override;

private:
    std::string name_{"ISA-TestCard"};

    // I/O port space (full 64K for test flexibility)
    std::unique_ptr<uint8_t[]> io_ = std::make_unique<uint8_t[]>(1 << 16);

    // DMA transfer buffer and state
    uint8_t dma_buf_[DMA_BUF_SIZE] = {};
    uint16_t dma_ptr_ = 0;
    uint8_t dma_irq_ = 5;  // IRQ to fire on DMA completion (default IRQ5)

    // Keyboard signals and component (optional)
    Signal* kbd_ready_ = nullptr;
    Signal* kbd_ack_ = nullptr;
    TestKeyboard* keyboard_ = nullptr;
};

} // namespace bench
