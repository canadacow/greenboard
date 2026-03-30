#include "isa/isa_testcard.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <ctime>

namespace bench {

ISA_TestCard::ISA_TestCard() {}

void ISA_TestCard::on_power_on() {
    dma_ptr_ = 0;
}

bool ISA_TestCard::claims_port(uint16_t port) {
    return (port & 0xFF80) == 0x0080;
}

uint8_t ISA_TestCard::on_io_read(uint16_t port) {
    if (port == 0xE0) return hfs_status_;
    if (port == 0xE1) {
        if (hfs_result_ptr_ < hfs_result_.size())
            return hfs_result_[hfs_result_ptr_++];
        return 0;
    }
    if (port == 0xE2) return static_cast<uint8_t>(hfs_result_.size() & 0xFF);
    if (port == 0xE3) return static_cast<uint8_t>((hfs_result_.size() >> 8) & 0xFF);
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
    } else if (port == 0xE0) {
        spdlog::info("[ISA-TestCard] port 0xE0 write: cmd=0x{:02X}", val);
        hfs_execute(val);
    } else if (port == 0xE1) {
        hfs_param_.push_back(val);
    } else if (port == 0xE4) {
        spdlog::info("[ISA-TestCard] port 0xE4 write: reset");
        hfs_param_.clear();
        hfs_result_.clear();
        hfs_result_ptr_ = 0;
    } else if (port == 0xE5) {
        spdlog::info("[HostFS] INT2F subfn=0x{:02X}", val);
    } else if (port == 0xE6) {
        spdlog::info("[HostFS] sda_seg_hi=0x{:02X}", val);
    } else if (port == 0xE7) {
        spdlog::info("[HostFS] sda_seg_lo=0x{:02X}", val);
    } else if (port == 0xE8) {
        spdlog::info("[HostFS] sda_off_hi=0x{:02X}", val);
    } else if (port == 0xE9) {
        spdlog::info("[HostFS] sda_off_lo=0x{:02X}", val);
    } else if (port == 0xEA) {
        spdlog::info("[HostFS] fn1[0]='{}' (0x{:02X})", (char)val, val);
    } else if (port == 0xEB) {
        spdlog::info("[HostFS] fn1[1]='{}' (0x{:02X})", (char)val, val);
    } else if (port == 0xEC) {
        spdlog::info("[HostFS] fn1[2]='{}' (0x{:02X})", (char)val, val);
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

// ===========================================================================
// HostFS -- host directory sharing via port I/O
// ===========================================================================

namespace fs = std::filesystem;

// Helper: extract null-terminated string from param buffer at offset.
static std::string hfs_get_string(const std::vector<uint8_t>& p, size_t off = 0) {
    std::string s;
    for (size_t i = off; i < p.size() && p[i]; ++i)
        s += static_cast<char>(p[i]);
    return s;
}

// Helper: read uint16_t from param buffer at offset.
static uint16_t hfs_get_u16(const std::vector<uint8_t>& p, size_t off) {
    if (off + 1 >= p.size()) return 0;
    return p[off] | (static_cast<uint16_t>(p[off + 1]) << 8);
}

// Helper: push uint16_t into result buffer.
static void hfs_put_u16(std::vector<uint8_t>& r, uint16_t v) {
    r.push_back(v & 0xFF);
    r.push_back((v >> 8) & 0xFF);
}

// Helper: push uint32_t into result buffer.
static void hfs_put_u32(std::vector<uint8_t>& r, uint32_t v) {
    r.push_back(v & 0xFF);
    r.push_back((v >> 8) & 0xFF);
    r.push_back((v >> 16) & 0xFF);
    r.push_back((v >> 24) & 0xFF);
}

// Helper: convert host path separators, resolve against root.
// DOS sends paths like "SUBDIR\FILE.TXT".
fs::path ISA_TestCard_resolve(const fs::path& root, const std::string& dos_path) {
    std::string p = dos_path;
    std::replace(p.begin(), p.end(), '\\', '/');
    // Strip leading drive letter if present (e.g. "E:\FOO" -> "FOO")
    if (p.size() >= 2 && p[1] == ':') p = p.substr(2);
    if (!p.empty() && p[0] == '/') p = p.substr(1);
    return root / p;
}

// Convert std::filesystem::file_time_type to DOS date/time.
static void to_dos_datetime(fs::file_time_type ft, uint16_t& date, uint16_t& time) {
    auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
        std::chrono::clock_cast<std::chrono::system_clock>(ft));
    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    date = static_cast<uint16_t>(
        ((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday);
    time = static_cast<uint16_t>(
        (tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2));
}

// Convert host file attributes to DOS attribute byte.
static uint8_t to_dos_attr(const fs::directory_entry& e) {
    uint8_t attr = 0;
    if (e.is_directory()) attr |= 0x10;
    if (e.is_regular_file()) {
        // Check read-only via permissions
        auto perms = e.status().permissions();
        if ((perms & fs::perms::owner_write) == fs::perms::none)
            attr |= 0x01;
    }
    return attr;
}

// Push a directory entry in DTA format (21 reserved + 32 bytes = 43 bytes):
//   [0]     attribute
//   [1-2]   time
//   [3-4]   date
//   [5-8]   size (32-bit)
//   [9-21]  filename.ext (8.3, space padded, null terminated)
// Convert a host filename to 8.3 DOS format. Returns false if not representable.
static bool to_dos_83(const std::string& host_name, char out[13]) {
    std::string stem, ext;
    auto dot = host_name.rfind('.');
    if (dot == std::string::npos || dot == 0) {
        stem = host_name;
    } else {
        stem = host_name.substr(0, dot);
        ext  = host_name.substr(dot + 1);
    }

    // Skip entries starting with '.' (except handled above)
    if (stem.empty()) return false;

    // Uppercase
    std::transform(stem.begin(), stem.end(), stem.begin(), ::toupper);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);

    // Truncate to 8.3
    if (stem.size() > 8) stem.resize(8);
    if (ext.size() > 3) ext.resize(3);

    // Strip characters illegal in DOS filenames
    for (auto& c : stem) {
        if (c == ' ' || c < 0x20) c = '_';
    }
    for (auto& c : ext) {
        if (c == ' ' || c < 0x20) c = '_';
    }

    // Build "NAME.EXT" or "NAME"
    int i = 0;
    for (char c : stem) out[i++] = c;
    if (!ext.empty()) {
        out[i++] = '.';
        for (char c : ext) out[i++] = c;
    }
    out[i] = 0;
    return true;
}

static void hfs_push_dir_entry(std::vector<uint8_t>& r, const fs::directory_entry& e) {
    uint8_t attr = to_dos_attr(e);
    uint16_t date, time;
    to_dos_datetime(e.last_write_time(), date, time);
    uint32_t size = e.is_regular_file() ? static_cast<uint32_t>(e.file_size()) : 0;

    char dos_name[13];
    std::string host_name = e.path().filename().string();
    if (!to_dos_83(host_name, dos_name)) {
        // Skip non-representable files (e.g. ".hidden")
        // Caller should handle this by trying next entry
        return;
    }

    r.push_back(attr);
    hfs_put_u16(r, time);
    hfs_put_u16(r, date);
    hfs_put_u32(r, size);
    for (int i = 0; dos_name[i]; ++i) r.push_back(static_cast<uint8_t>(dos_name[i]));
    r.push_back(0);
}

static const char* hfs_cmd_name(uint8_t cmd) {
    static const char* names[] = {
        "???", "FindFirst", "FindNext", "Open", "Close",
        "Read", "Write", "GetAttr", "ChDir", "DiskInfo",
        "Seek", "Create", "Mkdir", "Rmdir", "Delete", "Rename"
    };
    if (cmd < sizeof(names)/sizeof(names[0])) return names[cmd];
    return "???";
}

void ISA_TestCard::hfs_execute(uint8_t cmd) {
    hfs_result_.clear();
    hfs_result_ptr_ = 0;
    hfs_status_ = 1;  // assume ok

    // Only log param as string for commands that use string params
    if (cmd <= 0x03 || cmd == 0x07 || cmd == 0x08 || (cmd >= 0x0B && cmd <= 0x0F)) {
        std::string param_str(hfs_param_.begin(), hfs_param_.end());
        spdlog::info("[HostFS] cmd=0x{:02X} ({}) param=\"{}\" root={}",
                     cmd, hfs_cmd_name(cmd), param_str, hostfs_root_.string());
    } else {
        spdlog::info("[HostFS] cmd=0x{:02X} ({}) [{} bytes param]",
                     cmd, hfs_cmd_name(cmd), hfs_param_.size());
    }

    switch (cmd) {
        case 0x01: hfs_cmd_find_first(); break;
        case 0x02: hfs_cmd_find_next();  break;
        case 0x03: hfs_cmd_open();       break;
        case 0x04: hfs_cmd_close();      break;
        case 0x05: hfs_cmd_read();       break;
        case 0x06: hfs_cmd_write();      break;
        case 0x07: hfs_cmd_get_attr();   break;
        case 0x08: hfs_cmd_chdir();      break;
        case 0x09: hfs_cmd_get_disk_info(); break;
        case 0x0A: hfs_cmd_seek();       break;
        case 0x0B: hfs_cmd_create();     break;
        case 0x0C: hfs_cmd_mkdir();      break;
        case 0x0D: hfs_cmd_rmdir();      break;
        case 0x0E: hfs_cmd_delete();     break;
        case 0x0F: hfs_cmd_rename();     break;
        default:
            spdlog::warn("[HostFS] unknown cmd 0x{:02X}", cmd);
            hfs_status_ = 0xFF;
            break;
    }

    spdlog::info("[HostFS] -> status={} result_len={}", hfs_status_, hfs_result_.size());
}

// Match a DOS 8.3 filename against a DOS wildcard pattern.
// Both should be uppercase.  '?' matches any single char.
// Pattern like "EXPAN.???" or "????????.EXE" or "*.*".
// DOS expands '*' to '?' fills before sending to the redirector,
// e.g. "*.EXE" -> "????????.EXE", "*.*" -> "????????.???".
static bool dos_glob_match(const std::string& name, const std::string& pattern) {
    // Convert both to FCB-style 11-char format for comparison
    auto to_fcb = [](const std::string& s, char out[11]) {
        std::memset(out, ' ', 11);
        size_t dot = s.find('.');
        std::string stem = (dot == std::string::npos) ? s : s.substr(0, dot);
        std::string ext  = (dot == std::string::npos) ? "" : s.substr(dot + 1);
        // '*' in stem/ext fills remaining positions with '?' (per DOSBox)
        bool star = false;
        for (size_t i = 0; i < 8; i++) {
            if (i < stem.size() && stem[i] == '*') star = true;
            if (star) out[i] = '?'; else if (i < stem.size()) out[i] = stem[i];
        }
        star = false;
        for (size_t i = 0; i < 3; i++) {
            if (i < ext.size() && ext[i] == '*') star = true;
            if (star) out[8+i] = '?'; else if (i < ext.size()) out[8+i] = ext[i];
        }
    };

    char nfcb[11], pfcb[11];
    to_fcb(name, nfcb);
    to_fcb(pattern, pfcb);

    for (int i = 0; i < 11; i++) {
        if (pfcb[i] == '?') continue;
        if (pfcb[i] != nfcb[i]) return false;
    }
    return true;
}

void ISA_TestCard::hfs_cmd_find_first() {
    std::string pattern = hfs_get_string(hfs_param_);
    auto dir = ISA_TestCard_resolve(hostfs_root_, pattern);

    // Separate directory and glob (e.g. "SUBDIR\*.*" -> dir="SUBDIR", glob="*.*")
    auto parent = dir.parent_path();
    auto glob = dir.filename().string();
    std::transform(glob.begin(), glob.end(), glob.begin(), ::toupper);

    hfs_dir_entries_.clear();
    hfs_dir_idx_ = 0;

    std::error_code ec;
    for (auto& e : fs::directory_iterator(parent, ec)) {
        char dos_name[13];
        std::string host_name = e.path().filename().string();
        if (!to_dos_83(host_name, dos_name)) continue;
        std::string upper_name(dos_name);
        std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);
        if (dos_glob_match(upper_name, glob))
            hfs_dir_entries_.push_back(e);
    }

    if (ec || hfs_dir_entries_.empty()) {
        hfs_status_ = 0xFF;  // no files found
        return;
    }

    hfs_cmd_find_next();
}

void ISA_TestCard::hfs_cmd_find_next() {
    // Skip entries that can't be represented as 8.3
    while (hfs_dir_idx_ < hfs_dir_entries_.size()) {
        size_t before = hfs_result_.size();
        hfs_push_dir_entry(hfs_result_, hfs_dir_entries_[hfs_dir_idx_++]);
        if (hfs_result_.size() > before) return;  // got one
    }
    hfs_status_ = 0xFF;  // no more files
}

// Open and Create both return: handle(2), attr(1), time(2), date(2), size(4)
// to match what fill_sft() on the TSR side expects.
static void hfs_push_file_meta(std::vector<uint8_t>& r, uint16_t handle,
                                const fs::path& path) {
    hfs_put_u16(r, handle);

    std::error_code ec;
    auto entry = fs::directory_entry(path, ec);
    uint8_t attr = ec ? 0x20 : to_dos_attr(entry);
    r.push_back(attr);

    uint16_t date = 0, time = 0;
    if (!ec) to_dos_datetime(entry.last_write_time(), date, time);
    hfs_put_u16(r, time);
    hfs_put_u16(r, date);

    uint32_t size = 0;
    if (!ec && entry.is_regular_file()) size = static_cast<uint32_t>(entry.file_size());
    hfs_put_u32(r, size);
}

void ISA_TestCard::hfs_cmd_open() {
    std::string path_str = hfs_get_string(hfs_param_);
    auto path = ISA_TestCard_resolve(hostfs_root_, path_str);

    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f.is_open()) {
        f.open(path, std::ios::in | std::ios::binary);
    }
    if (!f.is_open()) {
        spdlog::warn("[HostFS] open failed: {}", path.string());
        hfs_status_ = 0xFF;
        return;
    }

    uint16_t h = hfs_next_handle_++;
    hfs_files_[h] = {std::move(f), path};
    hfs_push_file_meta(hfs_result_, h, path);
    spdlog::info("[HostFS] open {} -> handle {}", path.string(), h);
}

void ISA_TestCard::hfs_cmd_close() {
    uint16_t h = hfs_get_u16(hfs_param_, 0);
    auto it = hfs_files_.find(h);
    if (it != hfs_files_.end()) {
        spdlog::info("[HostFS] close handle {}", h);
        hfs_files_.erase(it);
    }
}

void ISA_TestCard::hfs_cmd_read() {
    uint16_t h = hfs_get_u16(hfs_param_, 0);
    uint16_t count = hfs_get_u16(hfs_param_, 2);

    auto it = hfs_files_.find(h);
    if (it == hfs_files_.end()) {
        hfs_status_ = 0xFF;
        return;
    }

    hfs_result_.resize(count);
    it->second.stream.read(reinterpret_cast<char*>(hfs_result_.data()), count);
    auto got = it->second.stream.gcount();
    hfs_result_.resize(static_cast<size_t>(got));

    if (got == 0 && it->second.stream.eof())
        hfs_status_ = 0xFF;  // EOF
}

void ISA_TestCard::hfs_cmd_write() {
    uint16_t h = hfs_get_u16(hfs_param_, 0);
    // Data starts at param offset 2
    auto it = hfs_files_.find(h);
    if (it == hfs_files_.end()) {
        hfs_status_ = 0xFF;
        return;
    }
    if (hfs_param_.size() <= 2) {
        hfs_put_u16(hfs_result_, 0); // 0-byte write is valid
        return;
    }
    size_t len = hfs_param_.size() - 2;
    it->second.stream.write(reinterpret_cast<const char*>(&hfs_param_[2]), len);
    hfs_put_u16(hfs_result_, static_cast<uint16_t>(len));
}

void ISA_TestCard::hfs_cmd_get_attr() {
    std::string path_str = hfs_get_string(hfs_param_);
    auto path = ISA_TestCard_resolve(hostfs_root_, path_str);

    std::error_code ec;
    auto entry = fs::directory_entry(path, ec);
    if (ec || !fs::exists(path, ec)) {
        hfs_status_ = 0xFF;
        return;
    }

    uint8_t attr = to_dos_attr(entry);
    uint32_t size = entry.is_regular_file() ? static_cast<uint32_t>(entry.file_size()) : 0;
    uint16_t date, time;
    to_dos_datetime(entry.last_write_time(), date, time);

    hfs_result_.push_back(attr);
    hfs_put_u32(hfs_result_, size);
    hfs_put_u16(hfs_result_, date);
    hfs_put_u16(hfs_result_, time);
}

void ISA_TestCard::hfs_cmd_chdir() {
    std::string path_str = hfs_get_string(hfs_param_);
    auto path = ISA_TestCard_resolve(hostfs_root_, path_str);
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        hfs_status_ = 0xFF;
        return;
    }
}

void ISA_TestCard::hfs_cmd_get_disk_info() {
    // Fake geometry matching DOS expectations:
    // Order: sectors/cluster, total_clusters, bytes/sector, free_clusters
    // -> TSR maps to AX=spc, BX=total, CX=bps, DX=free
    hfs_put_u16(hfs_result_, 64);    // sectors per cluster (AX)
    hfs_put_u16(hfs_result_, 1024);  // total clusters      (BX)
    hfs_put_u16(hfs_result_, 512);   // bytes per sector    (CX)
    hfs_put_u16(hfs_result_, 512);   // free clusters       (DX)
}

void ISA_TestCard::hfs_cmd_seek() {
    uint16_t h = hfs_get_u16(hfs_param_, 0);
    uint32_t offset = hfs_get_u16(hfs_param_, 2) |
                     (static_cast<uint32_t>(hfs_get_u16(hfs_param_, 4)) << 16);
    uint8_t whence = (hfs_param_.size() > 6) ? hfs_param_[6] : 0;

    auto it = hfs_files_.find(h);
    if (it == hfs_files_.end()) {
        hfs_status_ = 0xFF;
        return;
    }

    auto dir = std::ios::beg;
    if (whence == 1) dir = std::ios::cur;
    if (whence == 2) dir = std::ios::end;

    it->second.stream.clear();  // clear eofbit from prior reads
    it->second.stream.seekg(offset, dir);
    auto pos = static_cast<uint32_t>(it->second.stream.tellg());
    hfs_put_u32(hfs_result_, pos);
}

void ISA_TestCard::hfs_cmd_create() {
    std::string path_str = hfs_get_string(hfs_param_);
    auto path = ISA_TestCard_resolve(hostfs_root_, path_str);

    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        f.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    }
    if (!f.is_open()) {
        hfs_status_ = 0xFF;
        return;
    }

    uint16_t h = hfs_next_handle_++;
    hfs_files_[h] = {std::move(f), path};
    hfs_push_file_meta(hfs_result_, h, path);
    spdlog::info("[HostFS] create {} -> handle {}", path.string(), h);
}

void ISA_TestCard::hfs_cmd_mkdir() {
    std::string path_str = hfs_get_string(hfs_param_);
    auto path = ISA_TestCard_resolve(hostfs_root_, path_str);
    std::error_code ec;
    if (!fs::create_directory(path, ec)) hfs_status_ = 0xFF;
}

void ISA_TestCard::hfs_cmd_rmdir() {
    std::string path_str = hfs_get_string(hfs_param_);
    auto path = ISA_TestCard_resolve(hostfs_root_, path_str);
    std::error_code ec;
    if (!fs::remove(path, ec)) hfs_status_ = 0xFF;
}

void ISA_TestCard::hfs_cmd_delete() {
    std::string path_str = hfs_get_string(hfs_param_);
    auto path = ISA_TestCard_resolve(hostfs_root_, path_str);
    std::error_code ec;
    if (!fs::remove(path, ec)) hfs_status_ = 0xFF;
}

void ISA_TestCard::hfs_cmd_rename() {
    std::string old_str = hfs_get_string(hfs_param_, 0);
    std::string new_str = hfs_get_string(hfs_param_, old_str.size() + 1);
    auto old_path = ISA_TestCard_resolve(hostfs_root_, old_str);
    auto new_path = ISA_TestCard_resolve(hostfs_root_, new_str);
    std::error_code ec;
    fs::rename(old_path, new_path, ec);
    if (ec) hfs_status_ = 0xFF;
}

} // namespace bench
