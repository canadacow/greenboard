#include "core/threaded_component.h"
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace bench {

ThreadedComponent::ThreadedComponent(std::string name)
    : Component(std::move(name))
    , mailbox_(Mailbox::allocate())
{}

void ThreadedComponent::power_on() {
    if (thread_.joinable()) return;
    thread_ = std::jthread([this](std::stop_token stop) {
#ifdef _WIN32
        std::wstring wname(name().begin(), name().end());
        SetThreadDescription(GetCurrentThread(), wname.c_str());
#endif
        run(stop);
    });
    spdlog::debug("[{}] powered on", name());
}

void ThreadedComponent::power_off() {
    if (!thread_.joinable()) return;
    thread_.request_stop();
    // Wake the consumer so it can see the stop request.
    // Increment pending so this wake is balanced like any signal wake.
    Signal::pending.fetch_add(1, std::memory_order_release);
    mailbox_->wake();
    thread_.join();
    // Flush deferred ack + drain any queued wakes. Every semaphore
    // count has a matching pending increment, so ack them all.
    if (pending_ack_) {
        Signal::ack();
        pending_ack_ = false;
    }
    while (mailbox_->sem.try_acquire())
        Signal::ack();
    spdlog::debug("[{}] powered off", name());
}

// Default run loop for reactive components: block on mailbox, check pins.
void ThreadedComponent::run(std::stop_token stop) {
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
void ThreadedComponent::wait_mailbox(std::stop_token& stop) {
    if (pending_ack_) {
        int p = Signal::pending.load(std::memory_order_acquire);
        Signal::ack();
        pending_ack_ = false;
    }
    mailbox_->sem.acquire();
    pending_ack_ = true;
}

void ThreadedComponent::flush_pending_ack() {
    if (pending_ack_) {
        Signal::ack();
        pending_ack_ = false;
    }
}

bool ThreadedComponent::stop_requested() const {
    return !thread_.joinable() || thread_.get_stop_token().stop_requested();
}

void ThreadedComponent::subscribe_to(Signal& sig) {
    connected_signals_.push_back(&sig);
}

} // namespace bench
