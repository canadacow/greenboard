#pragma once
#include "isa/isa_adapter.h"
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
// Supports: READ DATA (multi-sector), SPECIFY, RECALIBRATE, SEEK,
// SENSE INTERRUPT STATUS, READ ID. Loads a raw disk image at construction.
class ISA_FloppyController final : public ISA_Adapter {
public:
    // disk_image: raw sector image (e.g. 360K .img file).
    // sectors_per_track, heads: geometry for CHS -> LBA translation.
    ISA_FloppyController(std::vector<uint8_t> disk_image,
                         int sectors_per_track = 9, int heads = 2);

    // Load a disk image after construction.
    void load_image(std::vector<uint8_t> img, int spt, int hds);

protected:
    void on_power_on() override;

    bool claims_port(uint16_t port) override;
    bool claims_mmio(uint32_t addr) override;
    uint8_t on_io_read(uint16_t port) override;
    void    on_io_write(uint16_t port, uint8_t val) override;
    uint8_t on_mmio_read(uint32_t addr) override;
    void    on_mmio_write(uint32_t addr, uint8_t val) override;
    uint8_t on_dma_read() override;
    void    on_dma_complete(int channel) override;

private:
    // Disk image
    std::vector<uint8_t> image_;
    int spt_ = 9;    // sectors per track
    int heads_ = 2;

    // FDC registers
    uint8_t dor_ = 0;    // Digital Output Register

    // FDC state machine
    enum class Phase { Idle, Command, Execution, Result };
    Phase phase_ = Phase::Idle;

    // Command buffer
    uint8_t cmd_buf_[9] = {};
    int cmd_len_ = 0;          // bytes received so far
    int cmd_expected_ = 0;     // total bytes expected for current command

    // Result buffer
    uint8_t result_buf_[7] = {};
    int result_len_ = 0;       // total result bytes
    int result_pos_ = 0;       // next result byte to return

    // Execution state (sector read/write -- shared by DMA and PIO)
    uint32_t sector_offset_ = 0;  // byte offset into image for current sector
    uint16_t sector_size_ = 512;
    uint16_t xfer_ptr_ = 0;       // bytes transferred so far within current sector
    bool pio_mode_ = false;        // true when DOR bit 3 is clear (no DMA)
    int cur_sector_ = 1;          // current 1-based sector number (advances multi-sector)
    int eot_ = 0;                 // end-of-track sector number from command

    // Current cylinder per drive (for SENSE INTERRUPT STATUS)
    uint8_t pcn_[4] = {};

    // Interrupt pending
    bool irq_pending_ = false;
    bool reset_sense_ = false;  // true = next SENSE INT returns 0xC0 (reset)

    // MSR computation
    uint8_t read_msr() const;

    // Command dispatch
    void start_command();
    void execute_read_data();
    void build_result_ok();
    bool advance_sector();  // move to next sector; returns false if past EOT

    // CHS -> byte offset
    uint32_t chs_to_offset(int cyl, int head, int sector) const;
};

} // namespace bench
