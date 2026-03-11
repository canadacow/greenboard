#pragma once
#include "core/types.h"
#include "core/mailbox.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace bench {

class Component;
class InlineComponent;

// A single named signal line -- a wire/trace on the motherboard.
//
// drive() updates the level and propagates to subscribers:
//   1. Sync (InlineComponent): called inline, immediately, in the caller's thread.
//   2. Async (ThreadedComponent): woken via mailbox semaphore.
//
// If sync subscribers exist, drive() asserts that a transaction is active.
// This enforces that combinational propagation always completes before
// any async subscriber wakes.
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

    // Reset level to HiZ (power-off). Wiring (subscribers) stays intact.
    void reset();

    // Subscribe/unsubscribe a component to signal change events.
    // Routes through Component::subscribe_to() for type-safe dispatch.
    void connect(Component* c);
    void disconnect(Component* c);

    // Type-specific subscriber registration (called by subscribe_to()).
    void add_async(Mailbox* mb);
    void add_sync(InlineComponent* ic);

    // Returns the first InlineComponent subscriber.
    // Asserts if none -- the caller must know the topology.
    InlineComponent& get_inline() const;

    // Global pending-signal counter. Incremented by drive() per subscriber,
    // decremented by ack(). Clock must not advance until quiescent.
    static std::atomic<int> pending;
    static void ack() { pending.fetch_sub(1, std::memory_order_release); }
    static void wait_quiescent() {
        while (pending.load(std::memory_order_acquire) > 0)
            ;
    }
    static void wait_quiescent(std::stop_token& stop) {
        while (pending.load(std::memory_order_acquire) > 0
               && !stop.stop_requested())
            ;
    }

private:
    std::string name_;
    std::atomic<Level> level_{Level::HiZ};
    Level pull_ = Level::HiZ;  // default: no pull, floats

    // Async subscribers: threaded ICs woken via mailbox.
    std::vector<Mailbox*> subscribers_;

    // Sync subscribers: inline ICs called in caller's thread.
    std::vector<InlineComponent*> sync_subscribers_;

    std::mutex sub_mutex_;  // only used by connect/disconnect (setup time)
};

// A bundle of N named signal lines (e.g. address bus SA0..SA19).
class Bus {
public:
    Bus(const std::string& prefix, int width);

    int width() const { return static_cast<int>(lines_.size()); }
    Signal& operator[](int i) { return *lines_[i]; }
    const Signal& operator[](int i) const { return *lines_[i]; }

    // Convenience: drive/release/reset all lines.
    void drive(uint32_t value);
    void release();
    void reset();
    uint32_t read() const;

    // Connect/disconnect a component to all lines in this bus.
    void connect(Component* c);
    void disconnect(Component* c);

private:
    std::vector<std::unique_ptr<Signal>> lines_;
};

} // namespace bench
