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
    }

    on_power_off();
}

// Blocking: ack previous wake (deferred), then sleep until next signal change.
// Deferred ack keeps pending > 0 during all processing, so the clock
// cannot advance until this component is truly blocked and ready.
void Component::wait_mailbox(std::stop_token& stop) {
    if (pending_ack_) {
        Signal::ack();
        pending_ack_ = false;
    }
    mailbox_->sem.acquire();
    pending_ack_ = true;
}

bool Component::stop_requested() const {
    return !thread_.joinable() || thread_.get_stop_token().stop_requested();
}

} // namespace bench
