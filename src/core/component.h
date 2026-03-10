#pragma once
#include "core/types.h"
#include "core/signal.h"
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

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
    // Thread-safe: may be called from any thread.
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
    std::queue<SignalEvent> mailbox_;
    std::mutex mtx_;
    std::condition_variable_any cv_;
};

} // namespace bench
