#pragma once
#include "isa/isa_card.h"
#include "isa/isa_bus.h"
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <vector>
#include <cstdint>

namespace bench {

// ISA_FloppyController -- NEC uPD765 / Intel 8272A floppy disk controller.
//
// Plugs into an 8-bit ISA slot. Uses DMA channel 2, IRQ 6.
//
// I/O ports (primary controller):
//   0x3F2  DOR   Digital Output Register (write)
//   0x3F4  MSR   Main Status Register (read)
//   0x3F5  FIFO  Data Register (command/result bytes)
//
// Supports: READ DATA, WRITE DATA, FORMAT TRACK (multi-sector, DMA + PIO),
// SPECIFY, RECALIBRATE, SEEK, SENSE INTERRUPT STATUS, READ ID.
// Loads a raw disk image at construction.
class ISA_FloppyController final : public ISA_Card {
public:
    // disk_image: raw sector image for drive 0 (A:).
    // sectors_per_track, heads: geometry for CHS -> LBA translation.
    ISA_FloppyController(std::vector<uint8_t> disk_image,
                         int sectors_per_track = 9, int heads = 2);

    const std::string& card_name() const override { return name_; }

    // Load a disk image into a drive (0=A, 1=B). Hot-swappable.
    void load_image(std::vector<uint8_t> img, int spt, int hds, int drive = 0);
    int num_drives() const;  // number of drives with media inserted

    void card_save(cereal::BinaryOutputArchive& ar) override { serialize(ar); }
    void card_load(cereal::BinaryInputArchive& ar) override {
        serialize(ar);
        for (int i = 0; i < MAX_DRIVES; ++i)
            detect_protection(i);
    }
    template <class Archive> void serialize(Archive& ar) {
        for (int i = 0; i < MAX_DRIVES; ++i)
            ar(drives_[i].image, drives_[i].spt, drives_[i].heads);
        ar(active_drive_, dor_, phase_,
           cereal::binary_data(cmd_buf_, sizeof(cmd_buf_)),
           cmd_len_, cmd_expected_,
           cereal::binary_data(result_buf_, sizeof(result_buf_)),
           result_len_, result_pos_, sector_offset_, sector_size_,
           xfer_ptr_, pio_mode_, cur_sector_, eot_, prot_read_,
           format_mode_, format_fill_, format_spt_, format_n_,
           format_fields_received_,
           cereal::binary_data(pcn_, sizeof(pcn_)),
           irq_pending_, dma_bytes_transferred_,
           reset_sense_, reset_sense_drive_,
           seek_complete_pending_, seek_complete_st0_, seek_complete_drive_);
    }

    // ISA_Card overrides
    void on_power_on() override;
    bool claims_port(uint16_t port) override;
    bool claims_mmio(uint32_t addr) override;
    uint8_t on_io_read(uint16_t port) override;
    void    on_io_write(uint16_t port, uint8_t val) override;
    uint8_t on_mmio_read(uint32_t addr) override;
    void    on_mmio_write(uint32_t addr, uint8_t val) override;
    uint8_t on_dma_read() override;
    void    on_dma_write(uint8_t val) override;
    void    on_dma_complete(int channel) override;

private:
    std::string name_{"ISA-FDC"};

    // Per-drive disk images (0=A, 1=B)
    static constexpr int MAX_DRIVES = 2;
    struct Drive {
        std::vector<uint8_t> image;
        int spt = 9;
        int heads = 2;
        // MicroProse booter protection: track 4 head 0 sector 1 on the
        // original disk is misformatted so reads fail with "no data" while
        // still streaming gap filler (0x43) into the DMA buffer. Detected
        // from the image at load time, not serialized.
        bool ms_prot = false;
        bool has_media() const { return !image.empty(); }
    };
    Drive drives_[MAX_DRIVES];
    int active_drive_ = 0;  // currently selected drive (from DOR/command)

    Drive& active() { return drives_[active_drive_ & 1]; }
    const Drive& active() const { return drives_[active_drive_ & 1]; }

    // FDC registers
    uint8_t dor_ = 0;

    // FDC state machine
    enum class Phase { Idle, Command, Execution, Result };
    Phase phase_ = Phase::Idle;

    // Command buffer
    uint8_t cmd_buf_[9] = {};
    int cmd_len_ = 0;
    int cmd_expected_ = 0;

    // Result buffer
    uint8_t result_buf_[7] = {};
    int result_len_ = 0;
    int result_pos_ = 0;

    // Execution state (sector read/write -- shared by DMA and PIO)
    uint32_t sector_offset_ = 0;
    uint16_t sector_size_ = 512;
    bool prot_read_ = false;  // active read is a protection-sector read
    uint16_t xfer_ptr_ = 0;
    bool pio_mode_ = false;
    int cur_sector_ = 1;
    int eot_ = 0;
    bool format_mode_ = false;
    uint8_t format_fill_ = 0;
    int format_spt_ = 0;
    int format_n_ = 0;
    int format_fields_received_ = 0;

    // Current cylinder per drive (for SENSE INTERRUPT STATUS)
    uint8_t pcn_[4] = {};

    // Interrupt pending
    bool irq_pending_ = false;
    uint32_t dma_bytes_transferred_ = 0;
    bool reset_sense_ = false;
    uint8_t reset_sense_drive_ = 0;

    // Seek/recalibrate completion (returned by SENSE before reset senses)
    bool seek_complete_pending_ = false;
    uint8_t seek_complete_st0_ = 0;
    uint8_t seek_complete_drive_ = 0;

    // MSR computation
    uint8_t read_msr() const;

    // Command dispatch
    void start_command();
    void execute_read_data();
    void execute_write_data();
    void execute_format_track();
    void build_result_ok();
    void build_result_error(uint8_t st1);
    bool advance_sector();
    void detect_protection(int drive);

    // CHS -> byte offset (uses active drive geometry)
    uint32_t chs_to_offset(int cyl, int head, int sector) const;
};

} // namespace bench
