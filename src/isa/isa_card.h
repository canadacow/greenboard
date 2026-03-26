#pragma once
#include <string>
#include <cstdint>

namespace bench {

class ISA_Bus;

// ISA_Card -- pure virtual interface for 8-bit ISA expansion cards.
//
// Cards implement device-specific logic (port claims, I/O handlers, DMA,
// MMIO). The ISA_Bus handles all bus protocol, signal tracking, and DAG
// registration. Cards are NOT Components -- the bus is the single
// CallbackComponent for the entire ISA subsystem.
//
// Cards access bus helpers (raise_irq, assert_drq, etc.) through the
// bus_ pointer, set automatically by ISA_Bus::insert_card().
class ISA_Card {
public:
    virtual ~ISA_Card() = default;

    // Card identity.
    virtual const std::string& card_name() const = 0;

    // Port/address claiming.
    virtual bool claims_port(uint16_t port) = 0;
    virtual bool claims_mmio(uint32_t addr) = 0;

    // I/O port handlers.
    virtual uint8_t on_io_read(uint16_t port) = 0;
    virtual void    on_io_write(uint16_t port, uint8_t val) = 0;

    // MMIO handlers.
    virtual uint8_t on_mmio_read(uint32_t addr) = 0;
    virtual void    on_mmio_write(uint32_t addr, uint8_t val) = 0;

    // DMA: provide next byte for device->memory transfer.
    virtual uint8_t on_dma_read() { return 0xFF; }

    // DMA: receive next byte for memory->device transfer.
    virtual void on_dma_write(uint8_t /*val*/) {}

    // DMA: terminal count fired for this channel.
    virtual void on_dma_complete(int /*channel*/) {}

    // Lifecycle.
    virtual void on_power_on() {}

    // Bus access (set by ISA_Bus::insert_card).
    ISA_Bus* bus() const { return bus_; }

protected:
    ISA_Bus* bus_ = nullptr;
    friend class ISA_Bus;
};

} // namespace bench
