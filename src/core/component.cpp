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
    sem_.release();  // wake consumer if blocking
    thread_.join();
    spdlog::debug("[{}] powered off", name_);
}

void Component::post(const SignalEvent& event) {
    // Claim a slot (lock-free, multiple producers).
    uint32_t slot = tail_.fetch_add(1, std::memory_order_acq_rel);
    ring_[slot & (kCapacity - 1)] = event;

    // Signal that one more slot is ready to read.
    committed_.fetch_add(1, std::memory_order_release);

    // Only wake if consumer is actually sleeping (avoids syscall).
    if (sleeping_.load(std::memory_order_acquire)) {
        sem_.release();
    }
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
    // Grab how many events are ready.
    uint32_t ready = committed_.load(std::memory_order_acquire);
    uint32_t to_read = ready - head_;
    if (to_read == 0 || to_read > kCapacity) return;

    for (uint32_t i = 0; i < to_read; ++i) {
        uint32_t idx = (head_ + i) & (kCapacity - 1);
        auto& e = ring_[idx];
        on_signal_change(*e.signal, e.old_level, e.new_level);
    }
    head_ = ready;
}

// Blocking: wait for at least one event, then drain all pending.
void Component::wait_mailbox(std::stop_token& stop) {
    // Mark ourselves as sleeping so producers know to wake us.
    sleeping_.store(true, std::memory_order_release);

    // Check if events arrived between last drain and setting the flag.
    if (committed_.load(std::memory_order_acquire) != head_) {
        sleeping_.store(false, std::memory_order_relaxed);
        drain_mailbox();
        return;
    }

    sem_.acquire();
    sleeping_.store(false, std::memory_order_relaxed);
    if (stop.stop_requested()) return;
    drain_mailbox();
}

bool Component::stop_requested() const {
    return !thread_.joinable() || thread_.get_stop_token().stop_requested();
}

} // namespace bench
