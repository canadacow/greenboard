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

void Component::run(std::stop_token stop) {
    on_power_on();

    while (!stop.stop_requested()) {
        SignalEvent event;
        {
            std::unique_lock lock(mtx_);
            cv_.wait(lock, stop, [this] { return !mailbox_.empty(); });
            if (stop.stop_requested()) break;
            event = mailbox_.front();
            mailbox_.pop();
        }
        on_signal_change(*event.signal, event.old_level, event.new_level);
    }

    on_power_off();
}

} // namespace bench
