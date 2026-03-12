#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <immintrin.h>
#include <cstring>

namespace bench {

class Component;
class InlineComponent;
class Scheduler;

// Global signal pool -- two contiguous arrays for cache-friendly commit.
// Signals allocate a slot at construction. The scheduler commits by
// copying pending[] -> current[] in a tight loop.
struct SignalPool {
    static constexpr int MAX_SIGNALS = 512;
    alignas(64) static Level current[MAX_SIGNALS];
    alignas(64) static Level pending[MAX_SIGNALS];
    static int count;

    static int allocate() { return count++; }

    // Copy pending -> current. Returns true if anything changed.
    // AVX2: compare + copy 32 bytes at a time. MAX_SIGNALS is
    // a multiple of 32, so we process the full array -- no tail.
    static bool commit() {
        int any = 0;
        for (int i = 0; i < MAX_SIGNALS; i += 32) {
            __m256i cur = _mm256_load_si256((__m256i*)(current + i));
            __m256i pen = _mm256_load_si256((__m256i*)(pending + i));
            __m256i eq = _mm256_cmpeq_epi8(cur, pen);
            int mask = _mm256_movemask_epi8(eq);
            if (mask != -1) {
                _mm256_store_si256((__m256i*)(current + i), pen);
                any = 1;
            }
        }
        return any != 0;
    }
};

// A single named signal line -- a wire/trace on the motherboard.
//
// Double-buffered via SignalPool: drive() writes pending[idx],
// level() reads current[idx]. No per-signal bookkeeping.
// Scheduler commits the entire pool each pass.
class Signal {
public:
    explicit Signal(std::string name);

    const std::string& name() const { return name_; }

    Level level() const { return *current_; }

    void drive(Level lvl) {
        if (lvl == *pending_) return;
        *pending_ = lvl;
    }

    // Release the signal (go Hi-Z, or to pull level if set).
    void release() { drive(pull_); }

    // Set a pull-up (High) or pull-down (Low) resistor on this signal.
    void set_pull(Level pull);
    Level pull() const { return pull_; }

    // Reset level to HiZ (power-off). Wiring (subscribers) stays intact.
    void reset();

    // Subscribe/unsubscribe a component to signal change events.
    void connect(Component* c);
    void disconnect(Component* c);

    // Set the global scheduler (call once at init).
    static void set_scheduler(Scheduler* s) { scheduler_ = s; }

    // Global pending-signal counter (legacy -- used by ThreadedComponent only).
    static std::atomic<int> pending_count;
    static void ack() { pending_count.fetch_sub(1, std::memory_order_release); }
    static void wait_quiescent() {
        while (pending_count.load(std::memory_order_acquire) > 0)
            ;
    }

private:
    std::string name_;
    Level* current_;    // -> SignalPool::current[idx]
    Level* pending_;    // -> SignalPool::pending[idx]
    Level pull_ = Level::HiZ;

    static Scheduler* scheduler_;

    friend class Scheduler;
    friend class InlineComponent;
};

// A bundle of N named signal lines (e.g. address bus SA0..SA19).
class Bus {
public:
    Bus(const std::string& prefix, int width);

    int width() const { return static_cast<int>(lines_.size()); }
    Signal& operator[](int i) { return *lines_[i]; }
    const Signal& operator[](int i) const { return *lines_[i]; }

    void drive(uint32_t value);
    void release();
    void reset();
    uint32_t read() const;

    void connect(Component* c);
    void disconnect(Component* c);

private:
    std::vector<std::unique_ptr<Signal>> lines_;
};

} // namespace bench
