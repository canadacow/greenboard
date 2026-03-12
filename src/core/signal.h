#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>

namespace bench {

class Component;
class InlineComponent;
class Scheduler;

// A single named signal line -- a wire/trace on the motherboard.
//
// Double-buffered model:
//   - drive() writes to pending_ and marks the signal dirty.
//   - level() reads from level_ (committed value).
//   - Scheduler::evaluate() commits pending -> level at each CLK edge,
//     evaluates inline ICs to fixed-point, and wakes all async components.
class Signal {
public:
    explicit Signal(std::string name);

    const std::string& name() const { return name_; }

    // Read the committed signal level.
    Level level() const { return level_; }

    // Drive the signal to a new level (writes to pending_).
    void drive(Level lvl);

    // Release the signal (go Hi-Z, or to pull level if set).
    void release();

    // Set a pull-up (High) or pull-down (Low) resistor on this signal.
    void set_pull(Level pull);
    Level pull() const { return pull_; }

    // Reset level to HiZ (power-off). Wiring (subscribers) stays intact.
    void reset();

    // Subscribe/unsubscribe a component to signal change events.
    void connect(Component* c);
    void disconnect(Component* c);

    // Commit pending_ -> level_. Returns true if the level changed.
    bool commit();

    // Set the global scheduler (call once at init).
    static void set_scheduler(Scheduler* s) { scheduler_ = s; }

    // Global pending-signal counter.
    static std::atomic<int> pending;
    static void ack() { pending.fetch_sub(1, std::memory_order_release); }
    static void wait_quiescent() {
        while (pending.load(std::memory_order_acquire) > 0)
            ;
    }

private:
    std::string name_;
    Level level_ = Level::HiZ;
    Level pending_ = Level::HiZ;
    bool dirty_ = false;
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
