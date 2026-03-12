#include "core/callback_component.h"
#include <spdlog/spdlog.h>

namespace bench {

CallbackComponent::CallbackComponent(std::string name)
    : Component(std::move(name))
{}

void CallbackComponent::power_on() {
    powered_ = true;
    on_power_on();
    spdlog::debug("[{}] powered on (callback)", name());
}

void CallbackComponent::power_off() {
    on_power_off();
    powered_ = false;
    spdlog::debug("[{}] powered off (callback)", name());
}

void CallbackComponent::subscribe_to(Signal& sig) {
    connected_signals_.push_back(&sig);
}

} // namespace bench
