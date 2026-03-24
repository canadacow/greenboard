#include "isa/isa_testcard.h"
#include <spdlog/spdlog.h>

namespace bench {

ISA_TestCard::ISA_TestCard() {}

void ISA_TestCard::on_power_on() {
    dma_ptr_ = 0;
}

bool ISA_TestCard::claims_port(uint16_t port) {
    return (port & 0xFF80) == 0x0080;
}

uint8_t ISA_TestCard::on_io_read(uint16_t port) {
    return io_[port];
}

void ISA_TestCard::on_io_write(uint16_t port, uint8_t val) {
    if (port == 0xF0) {
        for (int i = 0; i < 8; i++)
            if (val & (1 << i)) bus_->raise_irq(i);
    } else if (port == 0xF1) {
        for (int i = 0; i < 8; i++)
            if (val & (1 << i)) bus_->lower_irq(i);
    } else if (port == 0xF4) {
        if (val >= 1 && val <= 3) {
            dma_ptr_ = 0;
            bus_->assert_drq(val);
        }
    } else if (port == 0xF5) {
        if (val >= 1 && val <= 3)
            bus_->deassert_drq(val);
    } else if (port == 0xF6) {
        if (val >= 2 && val <= 7)
            dma_irq_ = val;
    } else if (port == 0xFB) {
        if (keyboard_)
            keyboard_->enqueue(val);
    } else if (port == 0xFC) {
        if (kbd_ready_)
            kbd_ready_->drive(val ? Level::High : Level::Low);
    } else if (port == 0xFD) {
        if (kbd_ack_)
            kbd_ack_->drive(Level::High);
    } else {
        io_[port] = val;
    }
}

uint8_t ISA_TestCard::on_dma_read() {
    uint8_t byte = dma_buf_[dma_ptr_ % DMA_BUF_SIZE];
    dma_ptr_++;
    return byte;
}

void ISA_TestCard::on_dma_complete(int /*channel*/) {
    if (dma_irq_ >= 2 && dma_irq_ <= 7)
        bus_->raise_irq(dma_irq_);
}

} // namespace bench
