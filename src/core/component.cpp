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
    mailbox_->sem.release();  // wake consumer if blocking
    thread_.join();
    spdlog::debug("[{}] powered off", name_);
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
    uint32_t ready = mailbox_->committed.load(std::memory_order_acquire);
    uint32_t to_read = ready - mailbox_->head;
    if (to_read == 0 || to_read > Mailbox::kCapacity) return;

    for (uint32_t i = 0; i < to_read; ++i) {
        uint32_t idx = (mailbox_->head + i) & (Mailbox::kCapacity - 1);
        auto& e = mailbox_->ring[idx];
        on_signal_change(*e.signal, e.old_level, e.new_level);
    }
    mailbox_->head = ready;
}

// Blocking: wait for at least one event, then drain all pending.
void Component::wait_mailbox(std::stop_token& stop) {
    mailbox_->sleeping.store(true, std::memory_order_release);

    if (mailbox_->committed.load(std::memory_order_acquire) != mailbox_->head) {
        mailbox_->sleeping.store(false, std::memory_order_relaxed);
        drain_mailbox();
        return;
    }

    mailbox_->sem.acquire();
    mailbox_->sleeping.store(false, std::memory_order_relaxed);
    if (stop.stop_requested()) return;
    drain_mailbox();
}

bool Component::stop_requested() const {
    return !thread_.joinable() || thread_.get_stop_token().stop_requested();
}

} // namespace bench
