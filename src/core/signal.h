#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace bench {

class Component;
class Signal;

// A signal change event delivered to a component's mailbox.
struct SignalEvent {
    Signal* signal;
    Level old_level;
    Level new_level;
};

// A single named signal line -- a wire/trace on the motherboard.
//
// This is the fundamental communication primitive. In real hardware,
// a copper trace connects multiple IC pins. When one IC drives the
// trace, every other IC sees the voltage change instantly.
//
// In our model, Signal is a mailbox. drive() updates the level and
// posts a SignalEvent to every subscribed Component's mailbox.
// Thread-safe: multiple IC threads may read; typically one drives.
class Signal {
public:
    explicit Signal(std::string name);

    const std::string& name() const { return name_; }
    Level level() const { return level_.load(std::memory_order_acquire); }

    // Drive the signal to a new level. Notifies all subscribers.
    void drive(Level lvl);

    // Release the signal (go Hi-Z, or to pull level if set).
    void release();

    // Set a pull-up (High) or pull-down (Low) resistor on this signal.
    // When all drivers release, the signal settles to this level.
    void set_pull(Level pull);
    Level pull() const { return pull_; }

    // Subscribe/unsubscribe a component to signal change events.
    void connect(Component* c);
    void disconnect(Component* c);

private:
    std::string name_;
    std::atomic<Level> level_{Level::HiZ};
    Level pull_ = Level::HiZ;  // default: no pull, floats

    std::vector<Component*> subscribers_;
    std::mutex sub_mutex_;
};

// A bundle of N named signal lines (e.g. address bus SA0..SA19).
class Bus {
public:
    Bus(const std::string& prefix, int width);

    int width() const { return static_cast<int>(lines_.size()); }
    Signal& operator[](int i) { return *lines_[i]; }
    const Signal& operator[](int i) const { return *lines_[i]; }

    // Convenience: drive/release all lines from an integer value.
    void drive(uint32_t value);
    void release();
    uint32_t read() const;

    // Connect/disconnect a component to all lines in this bus.
    void connect(Component* c);
    void disconnect(Component* c);

private:
    std::vector<std::unique_ptr<Signal>> lines_;
};

} // namespace bench
