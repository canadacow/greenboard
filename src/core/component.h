#pragma once
#include "core/types.h"
#include <string>
#include <vector>

namespace bench {

class Signal;

// Base class for anything that sits in a socket on the motherboard.
//
// Two concrete subclasses model the two kinds of IC:
//
//   ThreadedComponent -- sequential/clocked ICs (8088, 8284A, 8288, 8259A, ...).
//     Has its own thread and mailbox. Woken asynchronously by signal changes.
//
//   InlineComponent -- combinational logic (74S373, 74S138, 74S245).
//     No thread. Evaluated synchronously by the Scheduler at each CLK edge.
//
class Component {
public:
    explicit Component(std::string name);
    virtual ~Component();

    const std::string& name() const { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    // Power control. Subclass semantics differ:
    //   ThreadedComponent: starts/stops the worker thread.
    //   InlineComponent: resets internal state (no thread).
    virtual void power_on() = 0;
    virtual void power_off() = 0;
    virtual bool is_powered() const = 0;

protected:
    // Called when a connected signal changes.
    // rising/falling indicate which CLK half-cycle is active.
    // Full cycle: both true.  Half cycle: one at a time.
    virtual void on_signal_change(bool /*rising*/, bool /*falling*/) {}
    virtual void on_power_on() {}
    virtual void on_power_off() {}

    // Each subclass routes itself into the correct Signal subscriber list.
    virtual void subscribe_to(Signal& sig) = 0;

    std::vector<Signal*> connected_signals_;

    friend class Signal;

private:
    std::string name_;
};

} // namespace bench
