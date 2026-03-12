#include "core/fiber_component.h"
#include <spdlog/spdlog.h>

namespace bench {

FiberComponent::FiberComponent(std::string name)
    : Component(std::move(name))
{}

FiberComponent::~FiberComponent() {
    power_off();
}

void FiberComponent::power_on() {
    if (fiber_) return;
    alive_ = true;
    fiber_ = fiber_create(65536, fiber_entry, this);
    spdlog::debug("[{}] powered on (fiber)", name());
}

void FiberComponent::power_off() {
    if (!fiber_) return;
    on_power_off();
    fiber_delete(fiber_);
    fiber_ = nullptr;
    alive_ = false;
    spdlog::debug("[{}] powered off (fiber)", name());
}

void FiberComponent::resume(Fiber caller) {
    if (!fiber_ || !alive_) return;
    return_fiber_ = caller;
    fiber_switch(fiber_);
}

void FiberComponent::run() {
    on_power_on();
    for (;;) {
        yield();
        on_signal_change();
    }
}

void FiberComponent::yield() {
    fiber_switch(return_fiber_);
}

void FiberComponent::subscribe_to(Signal& sig) {
    connected_signals_.push_back(&sig);
}

void FiberComponent::fiber_entry(void* user_data) {
    auto* self = static_cast<FiberComponent*>(user_data);
    self->run();
    self->alive_ = false;
    // run() returned -- yield forever so we never fall off the fiber proc.
    for (;;)
        fiber_switch(self->return_fiber_);
}

} // namespace bench
