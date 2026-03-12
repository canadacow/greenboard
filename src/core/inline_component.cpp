#include "core/inline_component.h"
#include <spdlog/spdlog.h>

namespace bench {

InlineComponent::InlineComponent(std::string name)
    : Component(std::move(name))
{}

void InlineComponent::power_on() {
    powered_ = true;
    on_power_on();
    spdlog::debug("[{}] powered on (inline)", name());
}

void InlineComponent::power_off() {
    on_power_off();
    powered_ = false;
    spdlog::debug("[{}] powered off (inline)", name());
}

void InlineComponent::subscribe_to(Signal& sig) {
    connected_signals_.push_back(&sig);
}

} // namespace bench
