#include "core/component.h"
#include <spdlog/spdlog.h>

namespace bench {

Component::Component(std::string name) : name_(std::move(name)) {}

void Component::power_on() {
    if (thread_.joinable()) return;
    thread_ = std::jthread([this](std::stop_token stop) { run(stop); });
    spdlog::debug("[{}] powered on", name_);
}

void Component::power_off() {
    if (!thread_.joinable()) return;
    thread_.request_stop();
    cv_.notify_all();
    thread_.join();
    spdlog::debug("[{}] powered off", name_);
}

void Component::post(const SignalEvent& event) {
    {
        std::lock_guard lock(mtx_);
        mailbox_.push(event);
    }
    cv_.notify_one();
}

// Default run loop for reactive components: block on mailbox, dispatch events.
void Component::run(std::stop_token stop) {
    on_power_on();

    while (!stop.stop_requested()) {
        wait_mailbox(stop);
    }

    on_power_off();
}

// Non-blocking: process all pending mailbox events right now.
void Component::drain_mailbox() {
    std::queue<SignalEvent> batch;
    {
        std::lock_guard lock(mtx_);
        batch.swap(mailbox_);
    }
    while (!batch.empty()) {
        auto& event = batch.front();
        on_signal_change(*event.signal, event.old_level, event.new_level);
        batch.pop();
    }
}

// Blocking: wait for at least one event, then drain all pending.
void Component::wait_mailbox(std::stop_token& stop) {
    SignalEvent event;
    {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, stop, [this] { return !mailbox_.empty(); });
        if (stop.stop_requested()) return;
        event = mailbox_.front();
        mailbox_.pop();
    }
    on_signal_change(*event.signal, event.old_level, event.new_level);

    // Drain any remaining events that arrived while we were processing.
    drain_mailbox();
}

bool Component::stop_requested() const {
    return !thread_.joinable() || thread_.get_stop_token().stop_requested();
}

} // namespace bench
