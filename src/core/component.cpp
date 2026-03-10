#include "core/component.h"
#include <spdlog/spdlog.h>

namespace bench {

Component::Component(std::string name)
    : name_(std::move(name))
    , mailbox_(Mailbox::allocate())
{}

void Component::power_on() {
    if (thread_.joinable()) return;
    thread_ = std::jthread([this](std::stop_token stop) { run(stop); });
    spdlog::debug("[{}] powered on", name_);
}

void Component::power_off() {
    if (!thread_.joinable()) return;
    thread_.request_stop();
    mailbox_->wake();  // wake consumer if blocking
    thread_.join();
    spdlog::debug("[{}] powered off", name_);
}

// Default run loop for reactive components: block on mailbox, check pins.
void Component::run(std::stop_token stop) {
    on_power_on();

    while (!stop.stop_requested()) {
        wait_mailbox(stop);
        if (stop.stop_requested()) break;
        on_signal_change();
        Signal::ack();
    }

    on_power_off();
}

// Blocking: sleep until any connected signal changes.
void Component::wait_mailbox(std::stop_token& stop) {
    mailbox_->sem.acquire();
}

bool Component::stop_requested() const {
    return !thread_.joinable() || thread_.get_stop_token().stop_requested();
}

} // namespace bench
