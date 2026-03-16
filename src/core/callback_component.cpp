#include "core/callback_component.h"
#include <spdlog/spdlog.h>

namespace bench {

CallbackComponent::CallbackComponent(std::string name)
    : Component(std::move(name))
{}

void CallbackComponent::power_on() {
    powered_ = true;
    on_power_on();
}

void CallbackComponent::power_off() {
    on_power_off();
    powered_ = false;
}

void CallbackComponent::subscribe_to(Signal& sig) {
    connected_signals_.push_back(&sig);
}

} // namespace bench
