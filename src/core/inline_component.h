#pragma once
#include "core/component.h"
#include "core/signal.h"

namespace bench {

// A combinational logic IC that executes synchronously via the Scheduler.
//
// No thread, no mailbox. Evaluation is driven by Scheduler::evaluate()
// at each CLK edge: all inline ICs have on_signal_change() called
// repeatedly until outputs reach a fixed point.
class InlineComponent : public Component {
public:
    explicit InlineComponent(std::string name);
    ~InlineComponent() override = default;

    void power_on() override;
    void power_off() override;
    bool is_powered() const override { return powered_; }

protected:
    // Routes this component into the connected_signals_ list.
    void subscribe_to(Signal& sig) override;

private:
    bool powered_ = false;

    friend class Scheduler;
};

} // namespace bench
