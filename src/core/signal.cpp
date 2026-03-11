#include "core/signal.h"
#include "core/component.h"
#include "core/inline_component.h"
#include <spdlog/spdlog.h>
#include <cassert>

namespace bench {

// --- Signal ---

std::atomic<int> Signal::pending{0};

Signal::Signal(std::string name) : name_(std::move(name)) {}

static const char* lvl_str(Level l) {
    switch (l) {
    case Level::Low: return "Low";
    case Level::High: return "High";
    case Level::HiZ: return "HiZ";
    }
    return "?";
}

void Signal::drive(Level lvl) {
    Level old = level_.load(std::memory_order_acquire);
    if (lvl == old) return;
    level_.store(lvl, std::memory_order_release);

    spdlog::trace("[SIG] {} {} -> {} async={} sync={}", name_,
        lvl_str(old), lvl_str(lvl), subscribers_.size(), sync_subscribers_.size());

    // Sync propagation: inline components run immediately in caller's thread.
    for (auto* ic : sync_subscribers_) {
        if (ic->in_sync_) continue;  // re-entrancy guard
        ic->in_sync_ = true;
        ic->on_signal_change();
        ic->in_sync_ = false;
    }

    // Async propagation: defer wakes if a transaction is active.
    auto* txn = InlineComponent::active_transaction();
    if (txn) {
        for (auto* mb : subscribers_)
            txn->defer_wake(mb);
    } else {
        for (auto* mb : subscribers_) {
            pending.fetch_add(1, std::memory_order_release);
            mb->wake();
        }
    }
}

void Signal::release() {
    drive(pull_);
}

void Signal::reset() {
    level_.store(Level::HiZ, std::memory_order_release);
}

void Signal::set_pull(Level pull) {
    pull_ = pull;
    if (level_.load(std::memory_order_acquire) == Level::HiZ && pull != Level::HiZ) {
        drive(pull);
    }
}

void Signal::connect(Component* c) {
    c->subscribe_to(*this);
}

void Signal::disconnect(Component* c) {
    std::lock_guard<std::mutex> lock(sub_mutex_);
    {
        auto* ic = dynamic_cast<InlineComponent*>(c);
        if (ic) {
            sync_subscribers_.erase(
                std::remove(sync_subscribers_.begin(), sync_subscribers_.end(), ic),
                sync_subscribers_.end());
            return;
        }
    }
}

void Signal::add_async(Mailbox* mb) {
    std::lock_guard<std::mutex> lock(sub_mutex_);
    subscribers_.push_back(mb);
}

void Signal::add_sync(InlineComponent* ic) {
    std::lock_guard<std::mutex> lock(sub_mutex_);
    sync_subscribers_.push_back(ic);
}

InlineComponent& Signal::get_inline() const {
    assert(!sync_subscribers_.empty() && "no InlineComponent subscriber on this signal");
    return *sync_subscribers_[0];
}

// --- Bus ---

Bus::Bus(const std::string& prefix, int width) {
    lines_.reserve(width);
    for (int i = 0; i < width; ++i) {
        lines_.push_back(std::make_unique<Signal>(prefix + std::to_string(i)));
    }
}

void Bus::drive(uint32_t value) {
    for (int i = 0; i < width(); ++i) {
        lines_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
    }
}

void Bus::release() {
    for (auto& line : lines_)
        line->release();
}

void Bus::reset() {
    for (auto& line : lines_)
        line->reset();
}

uint32_t Bus::read() const {
    uint32_t val = 0;
    for (int i = 0; i < width(); ++i) {
        if (lines_[i]->level() == Level::High)
            val |= (1u << i);
    }
    return val;
}

void Bus::connect(Component* c) {
    for (auto& line : lines_)
        line->connect(c);
}

void Bus::disconnect(Component* c) {
    for (auto& line : lines_)
        line->disconnect(c);
}

} // namespace bench
