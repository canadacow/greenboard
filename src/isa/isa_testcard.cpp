#include "isa/isa_testcard.h"
#include <spdlog/spdlog.h>

namespace bench {

ISA_TestCard::ISA_TestCard()
    : ISA_Adapter("ISA-TestCard") {
    set_description("ISA Test Card");
}

void ISA_TestCard::on_power_on() {
    ISA_Adapter::on_power_on();
    dma_ptr_ = 0;
}

// =========================================================================
// Port/address claiming
// =========================================================================

bool ISA_TestCard::claims_port(uint16_t port) {
    return (port & 0xFF80) == 0x0080;   // ports 0x80-0xFF
}

bool ISA_TestCard::claims_mmio(uint32_t addr) {
    return addr >= MMIO_BASE && addr < MMIO_BASE + MMIO_SIZE;
}

// =========================================================================
// I/O handlers
// =========================================================================

uint8_t ISA_TestCard::on_io_read(uint16_t port) {
    return io_[port];
}

void ISA_TestCard::on_io_write(uint16_t port, uint8_t val) {
    if (port == 0xF0) {
        // Test trigger: drive IRQ lines High.
        for (int i = 0; i < 8; i++)
            if (val & (1 << i)) raise_irq(i);
    } else if (port == 0xF1) {
        // Test clear: drive IRQ lines Low.
        for (int i = 0; i < 8; i++)
            if (val & (1 << i)) lower_irq(i);
    } else if (port == 0xF4) {
        // DMA start on channel N (val = 1, 2, or 3).
        if (val >= 1 && val <= 3) {
            dma_ptr_ = 0;
            assert_drq(val);
        }
    } else if (port == 0xF5) {
        // DMA stop on channel N.
        if (val >= 1 && val <= 3)
            deassert_drq(val);
    } else if (port == 0xF6) {
        // Set DMA completion IRQ number (2-7).
        if (val >= 2 && val <= 7)
            dma_irq_ = val;
    } else if (port == 0xFB) {
        // Keyboard enqueue: write a raw scancode to the keyboard queue.
        if (keyboard_)
            keyboard_->enqueue(val);
    } else if (port == 0xFC) {
        // Keyboard ready: drive signal High to arm the keyboard.
        if (kbd_ready_)
            kbd_ready_->drive(val ? Level::High : Level::Low);
    } else if (port == 0xFD) {
        // Keyboard ACK: IRQ handler writes the scancode it just processed.
        // Pulse the ACK signal so the keyboard delivers the next key.
        if (kbd_ack_)
            kbd_ack_->drive(Level::High);
    } else {
        io_[port] = val;
    }
}

// =========================================================================
// MMIO handlers
// =========================================================================

uint8_t ISA_TestCard::on_mmio_read(uint32_t addr) {
    return mmio_[addr - MMIO_BASE];
}

void ISA_TestCard::on_mmio_write(uint32_t addr, uint8_t val) {
    mmio_[addr - MMIO_BASE] = val;
}

// =========================================================================
// DMA handlers
// =========================================================================

uint8_t ISA_TestCard::on_dma_read() {
    uint8_t byte = dma_buf_[dma_ptr_ % DMA_BUF_SIZE];
    dma_ptr_++;
    return byte;
}

void ISA_TestCard::on_dma_complete(int /*channel*/) {
    if (dma_irq_ >= 2 && dma_irq_ <= 7)
        raise_irq(dma_irq_);
}

} // namespace bench
