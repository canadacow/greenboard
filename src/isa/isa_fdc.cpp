#include "isa/isa_fdc.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace bench {

ISA_FloppyController::ISA_FloppyController(std::vector<uint8_t> disk_image,
                                           int sectors_per_track, int heads)
    : ISA_Adapter("ISA-FDC", 0x04, 0x40)  // DMA ch2 (bit 2), IRQ6 (bit 6)
    , image_(std::move(disk_image))
    , spt_(sectors_per_track)
    , heads_(heads)
{
    set_description("Floppy Disk Controller");
}

void ISA_FloppyController::load_image(std::vector<uint8_t> img, int spt, int hds) {
    image_ = std::move(img);
    spt_ = spt;
    heads_ = hds;
}

void ISA_FloppyController::on_power_on() {
    ISA_Adapter::on_power_on();
    dor_ = 0;
    phase_ = Phase::Idle;
    cmd_len_ = 0;
    cmd_expected_ = 0;
    result_len_ = 0;
    result_pos_ = 0;
    sector_offset_ = 0;
    xfer_ptr_ = 0;
    pio_mode_ = false;
    irq_pending_ = false;
}

// =========================================================================
// Port claiming
// =========================================================================

bool ISA_FloppyController::claims_port(uint16_t port) {
    return port >= 0x3F0 && port <= 0x3F7;
}

bool ISA_FloppyController::claims_mmio(uint32_t /*addr*/) {
    return false;
}

// =========================================================================
// MSR
// =========================================================================

uint8_t ISA_FloppyController::read_msr() const {
    uint8_t msr = 0;
    switch (phase_) {
        case Phase::Idle:
        case Phase::Command:
            msr = 0x80;  // RQM=1, DIO=0 (host->FDC)
            break;
        case Phase::Execution:
            if (pio_mode_)
                msr = 0xF0;  // RQM=1, DIO=1, NDMA=1, BUSY=1 (PIO: byte ready)
            else
                msr = 0x10;  // BUSY (DMA execution in progress)
            break;
        case Phase::Result:
            msr = 0xC0;  // RQM=1, DIO=1 (FDC->host)
            break;
    }
    return msr;
}

// =========================================================================
// I/O handlers
// =========================================================================

uint8_t ISA_FloppyController::on_io_read(uint16_t port) {
    switch (port) {
        case 0x3F4:  // MSR
            return read_msr();

        case 0x3F5:  // FIFO
            // PIO execution phase: return next sector byte.
            if (phase_ == Phase::Execution && pio_mode_) {
                uint8_t val = 0x00;
                if (sector_offset_ + xfer_ptr_ < image_.size())
                    val = image_[sector_offset_ + xfer_ptr_];
                xfer_ptr_++;
                if (xfer_ptr_ >= sector_size_) {
                    // All bytes read -- transition to result phase.
                    spdlog::info("[{}] PIO complete: {} bytes transferred", name(), xfer_ptr_);
                    build_result_ok();
                }
                return val;
            }
            // Result phase: return status bytes.
            if (phase_ == Phase::Result && result_pos_ < result_len_) {
                if (result_pos_ == 0)
                    lower_irq(6);
                uint8_t val = result_buf_[result_pos_++];
                spdlog::debug("[{}] FIFO read: result[{}]=0x{:02X}", name(), result_pos_ - 1, val);
                if (result_pos_ >= result_len_) {
                    phase_ = Phase::Idle;
                    spdlog::debug("[{}] result phase complete -> idle", name());
                }
                return val;
            }
            return 0xFF;

        default:
            return 0xFF;
    }
}

void ISA_FloppyController::on_io_write(uint16_t port, uint8_t val) {
    switch (port) {
        case 0x3F2: {  // DOR
            uint8_t old = dor_;
            dor_ = val;
            bool was_reset = !(old & 0x04);
            bool now_active = (val & 0x04) != 0;
            if (was_reset && now_active) {
                // Coming out of reset.
                phase_ = Phase::Idle;
                cmd_len_ = 0;
                irq_pending_ = false;
                spdlog::debug("[{}] DOR: out of reset, val=0x{:02X}", name(), val);
            } else if (!now_active) {
                // Entering reset.
                phase_ = Phase::Idle;
                cmd_len_ = 0;
                spdlog::debug("[{}] DOR: reset, val=0x{:02X}", name(), val);
            }
            break;
        }

        case 0x3F5: {  // FIFO (command phase)
            if (phase_ == Phase::Idle || phase_ == Phase::Command) {
                if (phase_ == Phase::Idle) {
                    // First byte: determine command and expected length.
                    phase_ = Phase::Command;
                    cmd_len_ = 0;
                    uint8_t cmd_id = val & 0x1F;  // mask MT/MF/SK bits
                    switch (cmd_id) {
                        case 0x06:  // READ DATA
                            cmd_expected_ = 9;
                            break;
                        case 0x08:  // SENSE INTERRUPT STATUS
                            cmd_expected_ = 1;
                            break;
                        case 0x03:  // SPECIFY
                            cmd_expected_ = 3;
                            break;
                        case 0x07:  // RECALIBRATE
                            cmd_expected_ = 2;
                            break;
                        case 0x0F:  // SEEK
                            cmd_expected_ = 3;
                            break;
                        default:
                            cmd_expected_ = 1;  // unknown: just eat 1 byte
                            spdlog::warn("[{}] unknown FDC command 0x{:02X}", name(), val);
                            break;
                    }
                }
                if (cmd_len_ < 9)
                    cmd_buf_[cmd_len_++] = val;
                spdlog::debug("[{}] FIFO write: cmd[{}]=0x{:02X} (expect {})",
                              name(), cmd_len_ - 1, val, cmd_expected_);
                if (cmd_len_ >= cmd_expected_)
                    start_command();
            }
            break;
        }

        default:
            break;
    }
}

uint8_t ISA_FloppyController::on_mmio_read(uint32_t /*addr*/) { return 0xFF; }
void ISA_FloppyController::on_mmio_write(uint32_t /*addr*/, uint8_t /*val*/) {}

// =========================================================================
// Command dispatch
// =========================================================================

void ISA_FloppyController::start_command() {
    uint8_t cmd_id = cmd_buf_[0] & 0x1F;
    switch (cmd_id) {
        case 0x06:  // READ DATA
            execute_read_data();
            break;

        case 0x08: {  // SENSE INTERRUPT STATUS
            // Return ST0 + current cylinder.
            result_buf_[0] = 0x20;  // ST0: seek end
            result_buf_[1] = 0x00;  // PCN (current cylinder)
            result_len_ = 2;
            result_pos_ = 0;
            phase_ = Phase::Result;
            spdlog::debug("[{}] SENSE INTERRUPT -> ST0=0x{:02X}", name(), result_buf_[0]);
            break;
        }

        case 0x03:   // SPECIFY
        case 0x07:   // RECALIBRATE
        case 0x0F: { // SEEK
            // These complete immediately (no result phase for SPECIFY/RECALIBRATE).
            if (cmd_id == 0x07 || cmd_id == 0x0F) {
                // Fire IRQ for seek completion (real FDC does this).
                if (dor_ & 0x08) {  // DMA/IRQ enabled
                    irq_pending_ = true;
                    raise_irq(6);
                }
            }
            phase_ = Phase::Idle;
            cmd_len_ = 0;
            spdlog::debug("[{}] command 0x{:02X} complete -> idle", name(), cmd_id);
            break;
        }

        default:
            phase_ = Phase::Idle;
            cmd_len_ = 0;
            break;
    }
}

void ISA_FloppyController::execute_read_data() {
    // Parse command bytes:
    //   [0] command  [1] HD|DS  [2] C  [3] H  [4] R  [5] N  [6] EOT  [7] GPL  [8] DTL
    int cyl    = cmd_buf_[2];
    int head   = cmd_buf_[3];
    int sector = cmd_buf_[4];
    int n      = cmd_buf_[5];  // sector size code: 2 = 512

    sector_size_ = (n == 0) ? 128 : (128u << n);
    sector_offset_ = chs_to_offset(cyl, head, sector);
    xfer_ptr_ = 0;

    spdlog::info("[{}] READ DATA: C={} H={} R={} N={} size={} offset=0x{:05X}",
                 name(), cyl, head, sector, n, sector_size_, sector_offset_);

    if (sector_offset_ + sector_size_ > image_.size()) {
        spdlog::error("[{}] READ DATA: sector beyond image end", name());
        // Set error in result and skip to result phase.
        std::memset(result_buf_, 0, sizeof(result_buf_));
        result_buf_[0] = 0x40;  // ST0: abnormal termination
        result_buf_[1] = 0x04;  // ST1: no data
        result_len_ = 7;
        result_pos_ = 0;
        phase_ = Phase::Result;
        return;
    }

    // Enter execution phase.
    phase_ = Phase::Execution;
    pio_mode_ = !(dor_ & 0x08);  // DOR bit 3 clear = PIO mode

    if (pio_mode_) {
        spdlog::info("[{}] PIO mode: {} bytes to transfer", name(), sector_size_);
        // CPU will poll MSR and read bytes from FIFO.
    } else {
        assert_drq(2);
    }
}

// =========================================================================
// DMA
// =========================================================================

uint8_t ISA_FloppyController::on_dma_read() {
    if (sector_offset_ + xfer_ptr_ < image_.size()) {
        uint8_t byte = image_[sector_offset_ + xfer_ptr_];
        xfer_ptr_++;
        return byte;
    }
    xfer_ptr_++;
    return 0x00;
}

void ISA_FloppyController::on_dma_complete(int /*channel*/) {
    spdlog::info("[{}] DMA complete: {} bytes transferred", name(), xfer_ptr_);
    build_result_ok();
}

void ISA_FloppyController::build_result_ok() {
    // Build result phase (7 bytes): ST0, ST1, ST2, C, H, R, N
    int cyl    = cmd_buf_[2];
    int head   = cmd_buf_[3];
    int sector = cmd_buf_[4];
    int n      = cmd_buf_[5];

    result_buf_[0] = 0x00;  // ST0: normal
    result_buf_[1] = 0x00;  // ST1: no errors
    result_buf_[2] = 0x00;  // ST2: no errors
    result_buf_[3] = static_cast<uint8_t>(cyl);
    result_buf_[4] = static_cast<uint8_t>(head);
    result_buf_[5] = static_cast<uint8_t>(sector + 1);  // next sector
    result_buf_[6] = static_cast<uint8_t>(n);
    result_len_ = 7;
    result_pos_ = 0;
    phase_ = Phase::Result;

    // Fire IRQ 6 (if DMA/IRQ enabled in DOR).
    if (dor_ & 0x08)
        raise_irq(6);
}

// =========================================================================
// CHS -> offset
// =========================================================================

uint32_t ISA_FloppyController::chs_to_offset(int cyl, int head, int sector) const {
    // Sector numbering is 1-based.
    uint32_t lba = (cyl * heads_ + head) * spt_ + (sector - 1);
    return lba * 512;  // always 512 bytes per physical sector
}

} // namespace bench
