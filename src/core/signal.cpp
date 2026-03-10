#include "core/signal.h"
#include "core/component.h"

namespace bench {

// --- Signal ---

std::atomic<int> Signal::pending{0};

Signal::Signal(std::string name) : name_(std::move(name)) {}

void Signal::drive(Level lvl) {
    Level old = level_.load(std::memory_order_acquire);
    if (lvl == old) return;
    prev_level_ = old;
    level_.store(lvl, std::memory_order_release);

    for (auto* mb : subscribers_) {
        pending.fetch_add(1, std::memory_order_release);
        mb->wake();
    }
}

void Signal::release() {
    // When released, settle to pull level (or HiZ if no pull).
    drive(pull_);
}

void Signal::set_pull(Level pull) {
    pull_ = pull;
    // If currently floating, apply the pull immediately.
    if (level_.load(std::memory_order_acquire) == Level::HiZ && pull != Level::HiZ) {
        drive(pull);
    }
}

void Signal::connect(Component* c) {
    std::lock_guard<std::mutex> lock(sub_mutex_);
    subscribers_.push_back(c->mailbox());
}

void Signal::disconnect(Component* c) {
    std::lock_guard<std::mutex> lock(sub_mutex_);
    auto* mb = c->mailbox();
    subscribers_.erase(
        std::remove(subscribers_.begin(), subscribers_.end(), mb),
        subscribers_.end());
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
    for (auto& line : lines_) {
        line->release();
    }
}

uint32_t Bus::read() const {
    uint32_t val = 0;
    for (int i = 0; i < width(); ++i) {
        if (lines_[i]->level() == Level::High) {
            val |= (1u << i);
        }
    }
    return val;
}

void Bus::connect(Component* c) {
    for (auto& line : lines_) {
        line->connect(c);
    }
}

void Bus::disconnect(Component* c) {
    for (auto& line : lines_) {
        line->disconnect(c);
    }
}

} // namespace bench
