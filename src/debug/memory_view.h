#pragma once
#include <cstdint>
#include <vector>
#include <functional>

namespace bench {

// Unified read-only view of the 8088's 20-bit physical address space.
//
// Components register address ranges at setup time. The debugger reads
// through this without knowing about DRAM xlat, ROM banks, MDA framebuffer,
// or any other addressing quirk.
//
// All reads go through registered callbacks -- zero cost when not in use.
class MemoryView {
public:
    using ReadFn = std::function<uint8_t(uint32_t addr)>;

    // Register a reader for [base, base+size). Later registrations for
    // overlapping ranges take priority (searched last-to-first).
    void map(uint32_t base, uint32_t size, ReadFn fn) {
        regions_.push_back({base, size, std::move(fn)});
    }

    // Read a single byte from the 20-bit address space.
    uint8_t read(uint32_t phys) const {
        phys &= 0xFFFFF;
        // Search in reverse so later maps override earlier ones.
        for (int i = static_cast<int>(regions_.size()) - 1; i >= 0; --i) {
            auto& r = regions_[i];
            if (phys >= r.base && phys < r.base + r.size)
                return r.fn(phys);
        }
        return 0xFF;  // unmapped
    }

    // Read N bytes into a buffer.
    void read(uint32_t phys, uint8_t* buf, int count) const {
        for (int i = 0; i < count; ++i)
            buf[i] = read((phys + i) & 0xFFFFF);
    }

private:
    struct Region {
        uint32_t base;
        uint32_t size;
        ReadFn fn;
    };
    std::vector<Region> regions_;
};

} // namespace bench
