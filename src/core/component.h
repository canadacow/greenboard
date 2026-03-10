#pragma once
#include "core/types.h"
#include "core/signal.h"
#include <string>
#include <thread>
#include <atomic>
#include <semaphore>

namespace bench {

// Base class for any IC or active component on the motherboard.
//
// Each component runs on its own thread. When a signal it's subscribed
// to changes, a SignalEvent is posted to its mailbox. The component's
// thread wakes up and calls on_signal_change().
//
// This mirrors real hardware: ICs react concurrently to signal changes
// on their input pins. The wires (Signals) are the communication mechanism.
//
// Reactive components (default): block on the mailbox, wake on signal changes.
// Active components (e.g. oscillators): override run() with their own loop
// and call drain_mailbox() periodically to process signal events.
//
// Mailbox: pre-allocated lock-free MPSC ring buffer. Multiple producer
// threads (Signal::drive) push events via atomic fetch_add on the write
// index. Single consumer thread pops via relaxed read index advance.
// A binary_semaphore provides the wake signal (no mutex/condvar).
class Component {
public:
    explicit Component(std::string name);
    virtual ~Component() { power_off(); }

    const std::string& name() const { return name_; }

    // Power control -- starts/stops the component's thread.
    void power_on();
    void power_off();
    bool is_powered() const { return thread_.joinable(); }

    // Called by Signal::drive() to deliver a signal change to this component.
    // Thread-safe: may be called from any thread (lock-free).
    void post(const SignalEvent& event);

protected:
    // Derived classes implement these.
    virtual void on_signal_change(Signal& signal, Level old_level, Level new_level) = 0;
    virtual void on_power_on() {}
    virtual void on_power_off() {}

    // Override for active components (oscillators, etc.) that need their own loop.
    // Default implementation blocks on the mailbox waiting for signal events.
    virtual void run(std::stop_token stop);

    // Non-blocking: process all pending mailbox events right now.
    // Active components call this inside their spin loop to handle
    // input signal changes (RDY, RES, etc.) without blocking.
    void drain_mailbox();

    // Block until at least one event arrives, then drain all pending.
    // Used by reactive components or for waiting on VCC.
    void wait_mailbox(std::stop_token& stop);

    // Check if stop has been requested on this component's thread.
    bool stop_requested() const;

private:
    std::string name_;
    std::jthread thread_;

    // Pre-allocated lock-free MPSC ring buffer.
    // Producers: atomic fetch_add on tail_ to claim a slot, then write.
    // Consumer: reads from head_, advances after processing.
    static constexpr uint32_t kCapacity = 256;  // must be power of 2
    SignalEvent ring_[kCapacity];
    alignas(64) std::atomic<uint32_t> tail_{0};   // next write slot (producers)
    alignas(64) uint32_t head_{0};                 // next read slot (consumer only)
    alignas(64) std::atomic<uint32_t> committed_{0}; // slots fully written

    // Lazy wake: producer only calls sem_.release() when consumer is sleeping.
    alignas(64) std::atomic<bool> sleeping_{false};
    std::binary_semaphore sem_{0};
};

} // namespace bench
