#include "isa/isa_fdc.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace bench {

ISA_FloppyController::ISA_FloppyController(std::vector<uint8_t> disk_image,
                                           int sectors_per_track, int heads)
{
    drives_[0].image = std::move(disk_image);
    drives_[0].spt = sectors_per_track;
    drives_[0].heads = heads;
}

void ISA_FloppyController::load_image(std::vector<uint8_t> img, int spt, int hds, int drive) {
    if (drive < 0 || drive >= MAX_DRIVES) return;
    drives_[drive].image = std::move(img);
    drives_[drive].spt = spt;
    drives_[drive].heads = hds;
}

int ISA_FloppyController::num_drives() const {
    int n = 0;
    for (int i = 0; i < MAX_DRIVES; ++i)
        if (drives_[i].has_media()) ++n;
    return n;
}

void ISA_FloppyController::on_power_on() {
    dor_ = 0;
    active_drive_ = 0;
    phase_ = Phase::Idle;
    cmd_len_ = 0;
    cmd_expected_ = 0;
    result_len_ = 0;
    result_pos_ = 0;
    sector_offset_ = 0;
    xfer_ptr_ = 0;
    cur_sector_ = 1;
    eot_ = 0;
    std::memset(pcn_, 0, sizeof(pcn_));
    pio_mode_ = false;
    irq_pending_ = false;
    reset_sense_ = false;
    reset_sense_drive_ = 0;
    format_mode_ = false;
    format_fill_ = 0;
    format_spt_ = 0;
    format_n_ = 0;
    format_fields_received_ = 0;
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
            msr = 0xD0;  // RQM=1, DIO=1, BUSY=1 (FDC->host)
            break;
    }
    return msr;
}

// =========================================================================
// I/O handlers
// =========================================================================

uint8_t ISA_FloppyController::on_io_read(uint16_t port) {
    switch (port) {
        case 0x3F4: {  // MSR
            uint8_t msr = read_msr();
            spdlog::info("[{}] MSR read: 0x{:02X} phase={}", name_, msr,
                phase_ == Phase::Idle ? "Idle" :
                phase_ == Phase::Command ? "Cmd" :
                phase_ == Phase::Execution ? "Exec" : "Result");
            return msr;
        }

        case 0x3F5:  // FIFO
            // PIO execution phase: return next sector byte.
            if (phase_ == Phase::Execution && pio_mode_) {
                uint8_t val = 0x00;
                if (sector_offset_ + xfer_ptr_ < active().image.size())
                    val = active().image[sector_offset_ + xfer_ptr_];
                xfer_ptr_++;
                if (xfer_ptr_ >= sector_size_) {
                    spdlog::info("[{}] PIO sector complete: sector {} ({} bytes)", name_, cur_sector_, xfer_ptr_);
                    if (advance_sector()) {
                        xfer_ptr_ = 0;  // next sector, keep reading
                    } else {
                        build_result_ok();
                    }
                }
                return val;
            }
            // Result phase: return status bytes.
            if (phase_ == Phase::Result && result_pos_ < result_len_) {
                if (result_pos_ == 0)
                    bus_->lower_irq(6);
                uint8_t val = result_buf_[result_pos_++];
                spdlog::info("[{}] result[{}]=0x{:02X} ({}/{})", name_,
                    result_pos_ - 1, val, result_pos_, result_len_);
                if (result_pos_ >= result_len_) {
                    phase_ = Phase::Idle;
                    spdlog::info("[{}] result phase complete -> Idle", name_);
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
            active_drive_ = val & 0x03;
            bool was_reset = !(old & 0x04);
            bool now_active = (val & 0x04) != 0;
            spdlog::info("[{}] DOR write: 0x{:02X} (old=0x{:02X}) drv={} motor={} dma={} reset={}",
                name_, val, old, val & 0x03, (val >> 4) & 0x0F,
                (val & 0x08) ? "on" : "off", (val & 0x04) ? "off" : "ON");
            if (was_reset && now_active) {
                spdlog::info("[{}] RESET release -> raising IRQ6", name_);
                phase_ = Phase::Idle;
                cmd_len_ = 0;
                irq_pending_ = true;
                reset_sense_ = true;
                reset_sense_drive_ = 0;  // real 765: poll drives 0-3
                if (val & 0x08)
                    bus_->raise_irq(6);
            } else if (!now_active) {
                // Entering reset -- NEC 765 deasserts interrupt output.
                // This ensures a clean rising edge when reset is released.
                phase_ = Phase::Idle;
                cmd_len_ = 0;
                bus_->lower_irq(6);
            }
            break;
        }

        case 0x3F5: {  // FIFO
            // PIO write execution: accept data bytes from CPU.
            if (phase_ == Phase::Execution && pio_mode_) {
                if (sector_offset_ + xfer_ptr_ < active().image.size())
                    active().image[sector_offset_ + xfer_ptr_] = val;
                xfer_ptr_++;
                if (xfer_ptr_ >= sector_size_) {
                    spdlog::info("[{}] PIO write sector complete: sector {} ({} bytes)", name_, cur_sector_, xfer_ptr_);
                    if (advance_sector()) {
                        xfer_ptr_ = 0;
                    } else {
                        build_result_ok();
                    }
                }
                break;
            }
            if (phase_ == Phase::Idle || phase_ == Phase::Command) {
                if (phase_ == Phase::Idle) {
                    // First byte: determine command and expected length.
                    phase_ = Phase::Command;
                    cmd_len_ = 0;
                    uint8_t cmd_id = val & 0x1F;  // mask MT/MF/SK bits
                    switch (cmd_id) {
                        case 0x05:  // WRITE DATA
                        case 0x06:  // READ DATA
                            cmd_expected_ = 9;
                            break;
                        case 0x0D:  // FORMAT TRACK
                            cmd_expected_ = 6;
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
                        case 0x0A:  // READ_ID
                            cmd_expected_ = 2;
                            break;
                        default:
                            cmd_expected_ = 1;  // unknown: just eat 1 byte
                            spdlog::warn("[{}] unknown FDC command 0x{:02X} (raw=0x{:02X})", name_, cmd_id, val);
                            break;
                    }
                }
                if (cmd_len_ < 9)
                    cmd_buf_[cmd_len_++] = val;
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
    // Commands with a HD|DS byte use it to select the active drive.
    if (cmd_expected_ >= 2)
        active_drive_ = cmd_buf_[1] & 0x03;
    switch (cmd_id) {
        case 0x05:  // WRITE DATA
            execute_write_data();
            break;

        case 0x06:  // READ DATA
            execute_read_data();
            break;

        case 0x0D:  // FORMAT TRACK
            execute_format_track();
            break;

        case 0x08: {  // SENSE INTERRUPT STATUS
            uint8_t drive;
            if (reset_sense_) {
                // uPD765: after reset, four sense interrupts queued (drives 0-3).
                drive = reset_sense_drive_++;
                result_buf_[0] = 0xC0 | drive;
                if (reset_sense_drive_ >= 4)
                    reset_sense_ = false;
            } else {
                drive = dor_ & 0x03;
                result_buf_[0] = 0x20 | drive;
            }
            result_buf_[1] = pcn_[drive];
            result_len_ = 2;
            result_pos_ = 0;
            phase_ = Phase::Result;
            break;
        }

        case 0x03:   // SPECIFY
        case 0x07:   // RECALIBRATE
        case 0x0F: { // SEEK
            if (cmd_id == 0x07) {
                uint8_t drive = cmd_buf_[1] & 0x03;
                pcn_[drive] = 0;
            }
            if (cmd_id == 0x0F) {
                uint8_t drive = cmd_buf_[1] & 0x03;
                pcn_[drive] = cmd_buf_[2];
            }
            if (cmd_id == 0x07 || cmd_id == 0x0F) {
                if (dor_ & 0x08) {
                    irq_pending_ = true;
                    bus_->raise_irq(6);
                }
            }
            phase_ = Phase::Idle;
            cmd_len_ = 0;
            break;
        }

        case 0x0A: {  // READ ID
            uint8_t drive = cmd_buf_[1] & 0x03;
            uint8_t head  = (cmd_buf_[1] >> 2) & 0x01;
            // Return the current position (next sector on the track).
            result_buf_[0] = 0x00;  // ST0: normal
            result_buf_[1] = 0x00;  // ST1
            result_buf_[2] = 0x00;  // ST2
            result_buf_[3] = pcn_[drive];
            result_buf_[4] = head;
            result_buf_[5] = 1;     // sector 1 (simulated -- head always at sector 1)
            result_buf_[6] = 2;     // N=2 (512 bytes)
            result_len_ = 7;
            result_pos_ = 0;
            phase_ = Phase::Result;
            if (dor_ & 0x08)
                bus_->raise_irq(6);
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
    cur_sector_ = sector;
    eot_ = cmd_buf_[6];
    sector_offset_ = chs_to_offset(cyl, head, sector);
    xfer_ptr_ = 0;

    spdlog::info("[{}] READ DATA: C={} H={} R={} N={} EOT={} size={} offset=0x{:05X}",
                 name_, cyl, head, sector, n, eot_, sector_size_, sector_offset_);

    if (sector_offset_ + sector_size_ > active().image.size()) {
        spdlog::error("[{}] READ DATA: sector beyond image end", name_);
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
        spdlog::info("[{}] PIO mode: {} bytes to transfer", name_, sector_size_);
    } else {
        bus_->assert_drq(2);
    }
}

void ISA_FloppyController::execute_write_data() {
    // Same parameter layout as READ DATA:
    //   [0] command  [1] HD|DS  [2] C  [3] H  [4] R  [5] N  [6] EOT  [7] GPL  [8] DTL
    int cyl    = cmd_buf_[2];
    int head   = cmd_buf_[3];
    int sector = cmd_buf_[4];
    int n      = cmd_buf_[5];

    sector_size_ = (n == 0) ? 128 : (128u << n);
    cur_sector_ = sector;
    eot_ = cmd_buf_[6];
    sector_offset_ = chs_to_offset(cyl, head, sector);
    xfer_ptr_ = 0;
    format_mode_ = false;

    spdlog::info("[{}] WRITE DATA: C={} H={} R={} N={} EOT={} size={} offset=0x{:05X}",
                 name_, cyl, head, sector, n, eot_, sector_size_, sector_offset_);

    if (sector_offset_ + sector_size_ > active().image.size()) {
        spdlog::error("[{}] WRITE DATA: sector beyond image end", name_);
        std::memset(result_buf_, 0, sizeof(result_buf_));
        result_buf_[0] = 0x40;
        result_buf_[1] = 0x04;
        result_len_ = 7;
        result_pos_ = 0;
        phase_ = Phase::Result;
        return;
    }

    phase_ = Phase::Execution;
    pio_mode_ = !(dor_ & 0x08);

    if (pio_mode_) {
        spdlog::info("[{}] PIO WRITE mode: {} bytes to transfer", name_, sector_size_);
    } else {
        bus_->assert_drq_write(2);
    }
}

void ISA_FloppyController::execute_format_track() {
    // FORMAT TRACK command bytes:
    //   [0] command  [1] HD|DS  [2] N  [3] SC (sectors/track)  [4] GPL  [5] D (fill byte)
    format_n_ = cmd_buf_[2];
    format_spt_ = cmd_buf_[3];
    format_fill_ = cmd_buf_[5];
    format_mode_ = true;
    format_fields_received_ = 0;

    // Use the drive's head from cmd_buf_[1] and current cylinder from PCN.
    uint8_t drive = cmd_buf_[1] & 0x03;
    uint8_t head  = (cmd_buf_[1] >> 2) & 0x01;
    int cyl = pcn_[drive];

    // Store C/H in cmd_buf_[2]/[3] so build_result_ok can reference them.
    cmd_buf_[2] = static_cast<uint8_t>(cyl);
    cmd_buf_[3] = head;
    cmd_buf_[5] = static_cast<uint8_t>(format_n_);

    sector_size_ = (format_n_ == 0) ? 128 : (128u << format_n_);
    cur_sector_ = format_spt_;
    eot_ = format_spt_;
    xfer_ptr_ = 0;

    spdlog::info("[{}] FORMAT TRACK: C={} H={} N={} SC={} fill=0x{:02X}",
                 name_, cyl, head, format_n_, format_spt_, format_fill_);

    phase_ = Phase::Execution;
    pio_mode_ = false;
    bus_->assert_drq_write(2);
}

// =========================================================================
// DMA
// =========================================================================

uint8_t ISA_FloppyController::on_dma_read() {
    uint8_t byte = 0x00;
    if (sector_offset_ + xfer_ptr_ < active().image.size())
        byte = active().image[sector_offset_ + xfer_ptr_];
    xfer_ptr_++;

    // Crossed a sector boundary? Advance to next sector so the DMA
    // controller can keep pulling bytes across multiple sectors.
    if (xfer_ptr_ >= sector_size_ && cur_sector_ < eot_) {
        advance_sector();
        xfer_ptr_ = 0;
    }
    return byte;
}

void ISA_FloppyController::on_dma_write(uint8_t val) {
    if (format_mode_) {
        // FORMAT TRACK: receive 4-byte address fields (C, H, R, N) per sector,
        // then fill the sector with the fill byte.
        // We ignore the address field content -- just count them.
        format_fields_received_++;
        if (format_fields_received_ % 4 == 0) {
            // Completed one address field (4 bytes). Fill a sector.
            int sec_idx = format_fields_received_ / 4;  // 1-based
            uint32_t offset = chs_to_offset(cmd_buf_[2], (cmd_buf_[1] >> 2) & 1, sec_idx);
            uint32_t sz = (format_n_ == 0) ? 128 : (128u << format_n_);
            if (offset + sz <= active().image.size())
                std::memset(&active().image[offset], format_fill_, sz);
        }
    } else {
        // WRITE DATA: store byte into image.
        if (sector_offset_ + xfer_ptr_ < active().image.size())
            active().image[sector_offset_ + xfer_ptr_] = val;
        xfer_ptr_++;

        // Multi-sector: advance when sector boundary crossed.
        if (xfer_ptr_ >= sector_size_ && cur_sector_ < eot_) {
            advance_sector();
            xfer_ptr_ = 0;
        }
    }
}

void ISA_FloppyController::on_dma_complete(int /*channel*/) {
    spdlog::debug("[{}] DMA complete", name_);
    format_mode_ = false;
    build_result_ok();
}

bool ISA_FloppyController::advance_sector() {
    if (cur_sector_ >= eot_)
        return false;  // reached end of track

    cur_sector_++;
    int cyl  = cmd_buf_[2];
    int head = cmd_buf_[3];
    sector_offset_ = chs_to_offset(cyl, head, cur_sector_);

    if (sector_offset_ + sector_size_ > active().image.size())
        return false;  // next sector beyond image

    return true;
}

void ISA_FloppyController::build_result_ok() {
    // Build result phase (7 bytes): ST0, ST1, ST2, C, H, R, N
    int cyl  = cmd_buf_[2];
    int head = cmd_buf_[3];
    int n    = cmd_buf_[5];

    result_buf_[0] = 0x00;  // ST0: normal
    result_buf_[1] = 0x00;  // ST1: no errors
    result_buf_[2] = 0x00;  // ST2: no errors
    result_buf_[3] = static_cast<uint8_t>(cyl);
    result_buf_[4] = static_cast<uint8_t>(head);
    result_buf_[5] = static_cast<uint8_t>(cur_sector_ + 1);  // next sector
    result_buf_[6] = static_cast<uint8_t>(n);
    result_len_ = 7;
    result_pos_ = 0;
    phase_ = Phase::Result;

    // Fire IRQ 6 (if DMA/IRQ enabled in DOR).
    if (dor_ & 0x08)
        bus_->raise_irq(6);
}

// =========================================================================
// CHS -> offset
// =========================================================================

uint32_t ISA_FloppyController::chs_to_offset(int cyl, int head, int sector) const {
    // Sector numbering is 1-based. Uses active drive geometry.
    const auto& d = drives_[active_drive_ & 1];
    uint32_t lba = (cyl * d.heads + head) * d.spt + (sector - 1);
    return lba * sector_size_;
}

} // namespace bench
