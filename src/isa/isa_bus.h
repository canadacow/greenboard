#pragma once
#include "core/callback_component.h"
#include "board/isa_slot.h"
#include "isa/isa_card.h"

namespace bench {

// ISA_Bus -- single CallbackComponent for the entire ISA expansion bus.
//
// Owns all bus signals (SD, SA, control, DMA, IRQ), runs the bus protocol
// once per cycle, and dispatches to the appropriate ISA_Card. Replaces the
// per-card ISA_Adapter pattern with one scheduler entry, one SD bidir
// block, and one set of edge trackers.
class ISA_Bus final : public CallbackComponent {
public:
    static constexpr int MAX_SLOTS = 5;

    ISA_Bus();

    // Wire to ISA slot signals. Call once after board wiring.
    void install(IsaSlot& slot);

    // Insert a card. dma_ch_mask: bitmask bits 1-3. irq_mask: bitmask bits 2-7.
    void insert_card(int slot_idx, ISA_Card* card,
                     uint8_t dma_ch_mask = 0, uint8_t irq_mask = 0);

    // Bus helpers -- called by cards through bus_ pointer.
    void raise_irq(int n);
    void lower_irq(int n);
    void assert_drq(int ch);          // device->memory (card drives SD)
    void assert_drq_write(int ch);    // memory->device (card reads SD)
    void deassert_drq(int ch);

    void on_cycle(Fiber caller) override;
    void on_power_on() override;

    // Debug: pin levels as the ISA bus sees them
    Level memr_level() const { return memr_.level(); }
    Level memw_level() const { return memw_.level(); }

private:
    // Slot cards
    ISA_Card* cards_[MAX_SLOTS] = {};
    int card_count_ = 0;

    // DMA channel -> card mapping (channels 1-3)
    ISA_Card* dma_owner_[4] = {};

    // IRQ line -> card mapping (IRQ 2-7)
    Signal* irq_sig_[8] = {};
    Pin irq_pin_[8]{};

    // ISA bus pins (wired once from any slot -- all slots share signals)
    Pin sd_[8]{};
    Pin sa_[20]{};
    Pin ior_{};
    Pin iow_{};
    Pin memr_{};
    Pin memw_{};
    Pin aen_{};

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
    bool dma_write_mode_ = false;
    ISA_Card* dma_active_card_ = nullptr;

    // Edge tracking
    Level ior_prev_ = Level::HiZ;
    Level iow_prev_ = Level::HiZ;
    Level dma_memr_prev_ = Level::HiZ;
    Level dma_memw_prev_ = Level::HiZ;
    Level cpu_memr_prev_ = Level::HiZ;
    Level cpu_memw_prev_ = Level::HiZ;
    bool mem_write_pending_ = false;
    bool data_driven_ = false;
    uint8_t read_byte_ = 0;
    bool write_pending_ = false;
    bool read_pending_ = false;

    // Helpers
    bool dma_active() const { return dma_active_ch_ >= 0; }
    uint32_t read_address();
    uint8_t  read_sd();
    void     drive_sd(uint8_t val);
    void     release_sd();

    // O(1) port/MMIO lookup tables, populated at insert_card time.
    // I/O: 10-bit address space (0x000-0x3FF) = 1024 entries.
    // MMIO: 4KB pages over first 1MB = 256 entries.
    static constexpr int IO_PORTS = 1024;
    static constexpr int MMIO_PAGES = 256;  // 1MB / 4KB
    ISA_Card* port_map_[IO_PORTS] = {};
    ISA_Card* mmio_map_[MMIO_PAGES] = {};

    ISA_Card* find_port_owner(uint16_t port) { return port < IO_PORTS ? port_map_[port] : nullptr; }
    ISA_Card* find_mmio_owner(uint32_t addr) { return addr < (MMIO_PAGES * 4096) ? mmio_map_[addr >> 12] : nullptr; }
};

} // namespace bench
