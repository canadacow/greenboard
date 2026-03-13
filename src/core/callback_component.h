#pragma once
#include "core/component.h"
#include "core/signal.h"

namespace bench {

// A clocked IC that needs per-tick evaluation but never yields.
//
// Sits between InlineComponent (fixed-point loop, combinational) and
// FiberComponent (cooperative multitasking, can suspend mid-operation).
//
// CallbackComponent is called once per evaluate(), after inlines settle,
// via a direct function call -- no fiber context switch overhead.
// Use this for ICs that react to clock edges / signal changes but
// complete all their work in a single on_signal_change() invocation.
//
// Pin direction declarations (declare_input/declare_output) feed the
// Scheduler's dependency graph. Callbacks that produce signals another
// callback consumes are placed in an earlier wave, with a commit()
// between waves so outputs are visible to consumers.
class CallbackComponent : public Component {
public:
    explicit CallbackComponent(std::string name);
    ~CallbackComponent() override = default;

    void power_on() override;
    void power_off() override;
    bool is_powered() const override { return powered_; }

    // declare_input/declare_output/inputs()/outputs() inherited from Component.

protected:
    void subscribe_to(Signal& sig) override;

private:
    bool powered_ = false;

    friend class Scheduler;
};

} // namespace bench
