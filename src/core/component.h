#pragma once
#include "core/types.h"
#include "core/signal.h"
#include <string>
#include <vector>

namespace bench {

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

    const std::string& description() const { return description_; }
    void set_description(std::string desc) { description_ = std::move(desc); }

    // Power control. Subclass semantics differ:
    //   ThreadedComponent: starts/stops the worker thread.
    //   InlineComponent: resets internal state (no thread).
    virtual void power_on() = 0;
    virtual void power_off() = 0;
    virtual bool is_powered() const = 0;

    // Pin direction declarations for wiring visualization and dependency graph.
    static constexpr int SLOT_WORDS = (SignalPool::MAX_SIGNALS + 63) / 64;
    void declare_input(Pin p)  { if (p.idx == 0) return; inputs_[p.idx / 64]  |= uint64_t(1) << (p.idx % 64); }
    void declare_output(Pin p) { if (p.idx == 0) return; outputs_[p.idx / 64] |= uint64_t(1) << (p.idx % 64); }

    // Async inputs are sampled by the IC at a future point (e.g. INTR, NMI,
    // READY, TEST on the 8088). They appear in the wiring graph as inputs
    // but create no ordering edges in the dependency DAG -- the value read
    // this cycle was driven in a previous cycle.
    void declare_async_input(Pin p) {
        if (p.idx == 0) return;
        inputs_[p.idx / 64]       |= uint64_t(1) << (p.idx % 64);
        async_inputs_[p.idx / 64] |= uint64_t(1) << (p.idx % 64);
    }

    const uint64_t* inputs()  const { return inputs_; }
    const uint64_t* outputs() const { return outputs_; }
    const uint64_t* async_inputs() const { return async_inputs_; }

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
    friend class Scheduler;

private:
    std::string name_;
    std::string description_;
    uint64_t inputs_[SLOT_WORDS]       = {};
    uint64_t outputs_[SLOT_WORDS]      = {};
    uint64_t async_inputs_[SLOT_WORDS] = {};
};

} // namespace bench
