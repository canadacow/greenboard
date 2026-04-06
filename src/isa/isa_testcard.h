#pragma once
#include "isa/isa_card.h"
#include "isa/isa_bus.h"
#include "test/test_keyboard.h"
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <vector>

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

    // HostFS -- shared directory mapped as a DOS drive via redirector TSR.
    // Ports 0xE0-0xEF.  See hostfs.asm for the DOS side.
    //
    // Protocol:
    //   Write 0xE4       = reset param pointer
    //   Write 0xE1 (seq) = append byte to param buffer
    //   Write 0xE0       = execute command (opcode in val)
    //   Read  0xE0       = status (0=busy, 1=ok, 0xFF=error)
    //   Read  0xE1 (seq) = next result byte
    //   Read  0xE2/0xE3  = result length low/high
    std::filesystem::path hostfs_root_;
    std::vector<uint8_t> hfs_param_;
    std::vector<uint8_t> hfs_result_;
    uint16_t hfs_result_ptr_ = 0;
    uint8_t  hfs_status_ = 1;  // 1=ok

    // Open file handles (DOS handle -> fstream)
    uint16_t hfs_next_handle_ = 1;
    struct HfsFile {
        std::fstream stream;
        std::filesystem::path path;
    };
    std::map<uint16_t, HfsFile> hfs_files_;

    // FindFirst/FindNext state
    std::vector<std::filesystem::directory_entry> hfs_dir_entries_;
    size_t hfs_dir_idx_ = 0;

    void hfs_execute(uint8_t cmd);
    void hfs_cmd_find_first();
    void hfs_cmd_find_next();
    void hfs_cmd_open();
    void hfs_cmd_close();
    void hfs_cmd_read();
    void hfs_cmd_write();
    void hfs_cmd_get_attr();
    void hfs_cmd_chdir();
    void hfs_cmd_get_disk_info();
    void hfs_cmd_seek();
    void hfs_cmd_create();
    void hfs_cmd_mkdir();
    void hfs_cmd_rmdir();
    void hfs_cmd_delete();
    void hfs_cmd_rename();

public:
    void card_save(cereal::BinaryOutputArchive& ar) override {
        ar(cereal::binary_data(io_.get(), 1 << 16));
        ar(cereal::binary_data(dma_buf_, sizeof(dma_buf_)));
        ar(dma_ptr_, dma_irq_);
        std::string root_str = hostfs_root_.string();
        ar(root_str);
        ar(hfs_param_, hfs_result_, hfs_result_ptr_, hfs_status_, hfs_next_handle_);
        // Save open file handles
        uint16_t count = static_cast<uint16_t>(hfs_files_.size());
        ar(count);
        for (auto& [handle, hf] : hfs_files_) {
            std::string path_str = hf.path.string();
            std::streampos pos = hf.stream.tellg();
            ar(handle, path_str, static_cast<uint64_t>(pos));
        }
        // Save dir entries
        uint32_t dir_count = static_cast<uint32_t>(hfs_dir_entries_.size());
        ar(dir_count);
        for (auto& entry : hfs_dir_entries_) {
            std::string p = entry.path().string();
            ar(p);
        }
        ar(hfs_dir_idx_);
    }
    void card_load(cereal::BinaryInputArchive& ar) override {
        ar(cereal::binary_data(io_.get(), 1 << 16));
        ar(cereal::binary_data(dma_buf_, sizeof(dma_buf_)));
        ar(dma_ptr_, dma_irq_);
        std::string root_str;
        ar(root_str);
        hostfs_root_ = root_str;
        ar(hfs_param_, hfs_result_, hfs_result_ptr_, hfs_status_, hfs_next_handle_);
        // Load open file handles
        hfs_files_.clear();
        uint16_t count;
        ar(count);
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t handle;
            std::string path_str;
            uint64_t pos;
            ar(handle, path_str, pos);
            auto& hf = hfs_files_[handle];
            hf.path = path_str;
            hf.stream.open(path_str, std::ios::in | std::ios::out | std::ios::binary);
            if (hf.stream) hf.stream.seekg(static_cast<std::streamoff>(pos));
        }
        // Load dir entries
        hfs_dir_entries_.clear();
        uint32_t dir_count;
        ar(dir_count);
        for (uint32_t i = 0; i < dir_count; ++i) {
            std::string p;
            ar(p);
            hfs_dir_entries_.emplace_back(std::filesystem::path(p));
        }
        ar(hfs_dir_idx_);
    }

    void set_hostfs_root(const std::filesystem::path& root) { hostfs_root_ = root; }
};

} // namespace bench
