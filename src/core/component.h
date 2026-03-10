#pragma once
#include "core/types.h"
#include "core/signal.h"
#include <string>
#include <thread>
#include <atomic>
#include <semaphore>

namespace bench {

class Signal;

// Base class for any IC or active component on the motherboard.
//
// Each component runs on its own thread. When a signal it's subscribed
// to changes, the component's mailbox semaphore is released. The
// component wakes and calls on_signal_change() to poll pin levels.
//
// This mirrors real hardware: ICs react concurrently to signal changes
// on their input pins. The wires (Signals) are the communication mechanism.
//
// Reactive components (default): block on the mailbox, wake on signal
// changes, call on_signal_change() to check pin levels.
// Active components (e.g. oscillators): override run() with their own loop.
class Component {
public:
    explicit Component(std::string name);
    virtual ~Component() { power_off(); }

    const std::string& name() const { return name_; }

    // Power control -- starts/stops the component's thread.
    void power_on();
    void power_off();
    bool is_powered() const { return thread_.joinable(); }

    // The mailbox for this component. Signals store this pointer.
    Mailbox* mailbox() const { return mailbox_; }

protected:
    // Called when woken from wait_mailbox. Check pin levels directly.
    virtual void on_signal_change() {}
    virtual void on_power_on() {}
    virtual void on_power_off() {}

    // Override for active components (oscillators, etc.) that need their own loop.
    // Default implementation blocks on the mailbox waiting for signal events.
    virtual void run(std::stop_token stop);

    // Block until a connected signal changes, then return.
    void wait_mailbox(std::stop_token& stop);

    // Check if stop has been requested on this component's thread.
    bool stop_requested() const;

private:
    std::string name_;
    std::jthread thread_;
    Mailbox* mailbox_;  // from static pool, outlives this Component
    bool pending_ack_ = false;  // deferred ack from previous wait_mailbox
};

} // namespace bench
