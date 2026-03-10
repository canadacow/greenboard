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
class Component {
public:
    explicit Component(std::string name);
    virtual ~Component() = default;

    const std::string& name() const { return name_; }

    // Power control -- starts/stops the component's thread.
    // jthread handles auto-join on destruction and stop_token for cancellation.
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

private:
    std::string name_;
    std::jthread thread_;
    std::queue<SignalEvent> mailbox_;
    std::mutex mtx_;
    std::condition_variable_any cv_;

    void run(std::stop_token stop);
};

} // namespace bench
