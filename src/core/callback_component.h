#pragma once
#include "core/component.h"
#include "core/signal.h"

namespace bench {

// An IC evaluated once per CLK edge via direct function call.
//
// No fiber context switch overhead. Use for any IC that completes
// all work in a single on_cycle() invocation.
//
// Pin direction declarations (declare_input/declare_output) feed the
// Scheduler's dependency DAG. Components are topologically sorted
// into waves so producers run before consumers.
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
