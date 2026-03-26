#pragma once
// CoroComponent -- base class for components driven by C++20 coroutines.
//
// Replaces FiberComponent for ICs that use coroutine-based execution
// (e.g. IC_8088 with BIU/EU coroutines). The scheduler treats these
// identically to FiberComponents for power management and DAG evaluation.
//
// Subclasses override power_on()/power_off() to create/destroy their
// coroutine(s), and on_cycle() to resume them.

#include "core/component.h"
#include "core/signal.h"

namespace bench {

class CoroComponent : public Component {
public:
    explicit CoroComponent(std::string name) : Component(std::move(name)) {}

protected:
    void subscribe_to(Signal& sig) override {
        connected_signals_.push_back(&sig);
    }
};

} // namespace bench
