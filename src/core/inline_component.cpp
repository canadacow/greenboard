#include "core/inline_component.h"
#include "core/signal.h"
#include <spdlog/spdlog.h>
#include <cassert>

namespace bench {

thread_local InlineComponent* InlineComponent::tl_active_txn_ = nullptr;

InlineComponent::InlineComponent(std::string name)
    : Component(std::move(name))
{}

void InlineComponent::power_on() {
    powered_ = true;
    begin_transaction();
    on_power_on();
    commit_transaction();
    spdlog::debug("[{}] powered on (inline)", name());
}

void InlineComponent::power_off() {
    begin_transaction();
    on_power_off();
    commit_transaction();
    powered_ = false;
    spdlog::trace("[{}] powered off (inline)", name());
}

void InlineComponent::subscribe_to(Signal& sig) {
    sig.add_sync(this);
    connected_signals_.push_back(&sig);
}

void InlineComponent::begin_transaction() {
    while (in_txn_.test_and_set(std::memory_order_acquire)) {
        // spin until latch acquired
    }
    tl_active_txn_ = this;
}

void InlineComponent::commit_transaction() {
    tl_active_txn_ = nullptr;
    in_txn_.clear(std::memory_order_release);

    for (auto* mb : deferred_wakes_) {
        Signal::pending.fetch_add(1, std::memory_order_release);
        mb->wake();
    }
    deferred_wakes_.clear();
}

void InlineComponent::defer_wake(Mailbox* mb) {
    deferred_wakes_.push_back(mb);
}

InlineComponent* InlineComponent::active_transaction() {
    return tl_active_txn_;
}

} // namespace bench
