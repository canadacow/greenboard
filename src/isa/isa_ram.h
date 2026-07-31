#pragma once
#include "isa/isa_card.h"
#include "isa/isa_bus.h"
#include "isa/ins8250.h"
#include "core/component.h"
#include <cereal/cereal.hpp>
#include <spdlog/spdlog.h>
#include <cstdint>
#include <cstring>

#include "debug/traced_writer.h"

namespace bench {

// AST SixPakPlus -- the classic 5150/XT multifunction card.
//
// Modeled functions:
//   - RAM expansion: 384KB at 0x40000-0x9FFFF (expands a 256KB 5150
//     to 640KB). Pure memory, no wait states.
//   - Two async serial ports (the optional second-serial kit
//     installed): COM1 at 3F8-3FF on IRQ4, COM2 at 2F8-2FF on IRQ3,
//     each an original INS8250 paced from the card's own 1.8432 MHz
//     crystal (free-running, expressed as a ratio to the bus OSC
//     reference -- baud rates are independent of host CPU speed,
//     exactly as on real hardware).
// Not modeled (yet): parallel port, clock/calendar, game port.
//
// Serial endpoints are pluggable behind the card (SerialDevice):
// null by default; set_com_device() attaches a mouse, host bridge, etc.
class ISA_RAM final : public ISA_Card, public Component
#if BENCH_CFG_TRACE
                    , public TracedWriter
#endif
{
public:
    // base: first byte address (must be 4KB-aligned for MMIO page map).
    // size: number of bytes (must be 4KB-aligned).
    ISA_RAM(uint32_t base, uint32_t size)
        : Component("SixPakPlus"), base_(base), size_(size) {
        ram_ = new uint8_t[size];
        std::memset(ram_, 0, size);
        uart_[0].reset();
        uart_[1].reset();
    }

    ~ISA_RAM() override { delete[] ram_; }

    const std::string& card_name() const override { return name_; }

    // Component overrides (clocked by ISA_Bus).
    void power_on() override { on_power_on(); }
    void power_off() override {}
    bool is_powered() const override { return true; }
    void on_cycle(Fiber) override {
        // The card's own 1.8432 MHz UART crystal free-runs against the
        // bus; pace it as a ratio to the bus OSC reference (3 OSC per
        // CLK). No CPU clock rate is assumed anywhere.
        xtal_acc_ += ISA_Bus::OSC_PER_CLK * XTAL_HZ;
        uint32_t cycles = xtal_acc_ / ISA_Bus::OSC_HZ;
        xtal_acc_ -= cycles * ISA_Bus::OSC_HZ;
        if (cycles) {
            uart_[0].tick(cycles);
            uart_[1].tick(cycles);
        }
        // IRQ lines follow the UARTs' gated interrupt outputs.
        bool i4 = uart_[0].irq_pending();
        bool i3 = uart_[1].irq_pending();
        if (bus()) {
            if (i4 != irq4_) { i4 ? bus()->raise_irq(4) : bus()->lower_irq(4); }
            if (i3 != irq3_) { i3 ? bus()->raise_irq(3) : bus()->lower_irq(3); }
        }
        irq4_ = i4;
        irq3_ = i3;
    }
protected:
    void subscribe_to(Signal&) override {}
public:

    void on_power_on() override {
        uart_[0].reset();
        uart_[1].reset();
        xtal_acc_ = 0;
        irq4_ = irq3_ = false;
    }

    // Attach the far end of a COM port's cable (0 = COM1, 1 = COM2).
    // Devices are not owned and not serialized; rebind after load.
    void set_com_device(int port, SerialDevice* dev) {
        if (port == 0 || port == 1)
            uart_[port].set_device(dev);
    }

    bool claims_port(uint16_t port) override {
        return (port >= 0x3F8 && port <= 0x3FF) ||
               (port >= 0x2F8 && port <= 0x2FF);
    }
    bool claims_mmio(uint32_t addr) override {
        return addr >= base_ && addr < base_ + size_;
    }

    uint8_t on_io_read(uint16_t port) override {
        if (port >= 0x3F8 && port <= 0x3FF)
            return uart_[0].read(port & 7);
        if (port >= 0x2F8 && port <= 0x2FF)
            return uart_[1].read(port & 7);
        return 0xFF;
    }
    void on_io_write(uint16_t port, uint8_t val) override {
        if (port >= 0x3F8 && port <= 0x3FF)
            uart_[0].write(port & 7, val);
        else if (port >= 0x2F8 && port <= 0x2FF)
            uart_[1].write(port & 7, val);
    }

    uint8_t on_mmio_read(uint32_t addr) override {
        return ram_[addr - base_];
    }
    void on_mmio_write(uint32_t addr, uint8_t val) override {
        ram_[addr - base_] = val;
#if BENCH_CFG_TRACE
        // Expansion RAM above the motherboard's 256KB. Reached by both CPU
        // stores and DMA transfers, same as DRAM.
        trace_write(addr, val);
#endif
    }

    void card_save(cereal::BinaryOutputArchive& ar) override {
        ar(base_, size_);
        ar(cereal::binary_data(ram_, size_));
        ar(uart_[0], uart_[1], xtal_acc_, irq4_, irq3_);
    }
    void card_load(cereal::BinaryInputArchive& ar) override {
        uint32_t b, s;
        ar(b, s);
        if (s != size_) { delete[] ram_; size_ = s; ram_ = new uint8_t[size_]; }
        base_ = b;
        ar(cereal::binary_data(ram_, size_));
        ar(uart_[0], uart_[1], xtal_acc_, irq4_, irq3_);
    }

    // Direct access for debugger memory view.
    const uint8_t* data() const { return ram_; }
    uint32_t base() const { return base_; }
    uint32_t size() const { return size_; }

private:
    // The card's own crystal (soldered next to the UARTs).
    static constexpr uint32_t XTAL_HZ = 1843200;

    std::string name_{"SixPakPlus"};
    uint32_t base_;
    uint32_t size_;
    uint8_t* ram_;

    INS8250 uart_[2];       // [0] = COM1 (3F8/IRQ4), [1] = COM2 (2F8/IRQ3)
    uint32_t xtal_acc_ = 0; // Hz accumulator for the 1.8432 MHz crystal
    bool irq4_ = false;
    bool irq3_ = false;
};

} // namespace bench
