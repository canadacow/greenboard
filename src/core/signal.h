#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <cassert>
#include <cstring>

namespace bench {

class Component;
class Scheduler;

// Global signal pool -- single flat array. Signals allocate a slot at
// construction. drive() writes directly, level() reads directly.
struct SignalPool {
    static constexpr int MAX_SIGNALS = 512;
    alignas(64) static Level levels[MAX_SIGNALS];
    static const char* names[MAX_SIGNALS];
    static int count;

    static int allocate() { return count++; }

#ifdef BENCH_PIN_VALIDATION
    static constexpr int SLOT_WORDS = (MAX_SIGNALS + 63) / 64;
    static Component* active_comp_;
    static uint64_t valid_write_[SLOT_WORDS];
    static uint64_t valid_read_[SLOT_WORDS];
    static uint64_t valid_hiz_release_[SLOT_WORDS];

    static thread_local bool is_clock_thread_;
    static void set_clock_thread() { is_clock_thread_ = true; }

    static bool validation_enabled_;
    static void enable_validation()  { validation_enabled_ = true; }
    static void disable_validation() { validation_enabled_ = false; }

    static void begin_component(Component* c);
    static void end_component() { active_comp_ = nullptr; }

    static void check_write(int idx, Level lvl);
    static void check_read(int idx);
#endif

};

// Lightweight handle into the SignalPool. Stores an index, not a pointer.
// drive()/level() use static array base + index = no pointer chase.
// Default-constructed Pin targets slot 0 (dummy: reads HiZ, writes vanish).
struct Pin {
    int idx = 0;

    Level level() const {
#ifdef BENCH_PIN_VALIDATION
        SignalPool::check_read(idx);
#endif
        return SignalPool::levels[idx];
    }
    void drive(Level lvl) {
#ifdef BENCH_PIN_VALIDATION
        SignalPool::check_write(idx, lvl);
#endif
        SignalPool::levels[idx] = lvl;
    }
    void release() {
#ifdef BENCH_PIN_VALIDATION
        SignalPool::check_write(idx, Level::HiZ);
#endif
        SignalPool::levels[idx] = Level::HiZ;
    }

    // Alias for drive() -- no double buffering, so immediate is the default.
    void drive_immediate(Level lvl) { drive(lvl); }
};

// Contiguous block of N pool slots. Stores one base index; read/write
// the whole block with memcpy/memset at compile-time-known width.
// Default base 0 targets the dummy slot (writes vanish, reads HiZ).
template<int N>
struct PinBlock {
    int base = 0;

    void drive(const Level* src) {
#ifdef BENCH_PIN_VALIDATION
        for (int i = 0; i < N; ++i) SignalPool::check_write(base + i, src[i]);
#endif
        std::memcpy(&SignalPool::levels[base], src, N);
    }
    void read(Level* dst) const {
#ifdef BENCH_PIN_VALIDATION
        for (int i = 0; i < N; ++i) SignalPool::check_read(base + i);
#endif
        std::memcpy(dst, &SignalPool::levels[base], N);
    }
    void fill(Level lvl) {
#ifdef BENCH_PIN_VALIDATION
        for (int i = 0; i < N; ++i) SignalPool::check_write(base + i, lvl);
#endif
        std::memset(&SignalPool::levels[base], static_cast<uint8_t>(lvl), N);
    }
    void release() { fill(Level::HiZ); }

    // Single-element access when needed.
    Level level(int i) const {
#ifdef BENCH_PIN_VALIDATION
        SignalPool::check_read(base + i);
#endif
        return SignalPool::levels[base + i];
    }
    void drive(int i, Level lvl) {
#ifdef BENCH_PIN_VALIDATION
        SignalPool::check_write(base + i, lvl);
#endif
        SignalPool::levels[base + i] = lvl;
    }

    // Build from socket pin array, asserting contiguity. Defined after Signal.
    template<typename Socket>
    static PinBlock from_socket(Socket& socket, const int (&pins)[N]);
};

// A single named signal line -- a wire/trace on the motherboard.
//
// Backed by a single slot in SignalPool::levels[]. drive() and level()
// hit the same array entry -- no double buffering.
class Signal {
public:
    explicit Signal(std::string name);

    const std::string& name() const { return name_; }

    Level level() const { return *level_; }

    void drive(Level lvl) {
        *level_ = lvl;
    }

    // Raw pool pointer -- for hot-path ICs that cache directly.
    Level* level_ptr() const { return level_; }

    // Index-based handle -- no pointer chase on the hot path.
    Pin pin() const { return Pin{static_cast<int>(level_ - SignalPool::levels)}; }

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
    Level* level_;      // -> SignalPool::levels[idx]
    Level pull_ = Level::HiZ;

    static Scheduler* scheduler_;

    friend class Scheduler;
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

// -- PinBlock::from_socket (needs Signal to be complete) --
template<int N>
template<typename Socket>
PinBlock<N> PinBlock<N>::from_socket(Socket& socket, const int (&pins)[N]) {
    PinBlock pb;
    Signal* s0 = socket.pin_signal(pins[0]);
    if (!s0) return pb;
    pb.base = s0->pin().idx;
    for (int i = 1; i < N; ++i) {
        Signal* s = socket.pin_signal(pins[i]);
        assert(s && s->pin().idx == pb.base + i &&
               "PinBlock: signals must map to contiguous pool slots");
    }
    return pb;
}

} // namespace bench
