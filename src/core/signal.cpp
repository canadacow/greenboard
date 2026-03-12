#include "core/signal.h"
#include "core/component.h"
#include "core/inline_component.h"
#include "core/scheduler.h"
#include <spdlog/spdlog.h>
#include <cassert>

namespace bench {

// --- Signal ---

std::atomic<int> Signal::pending{0};
Scheduler* Signal::scheduler_ = nullptr;

Signal::Signal(std::string name) : name_(std::move(name)) {}

Level Signal::level() const {
    return level_.load(std::memory_order_acquire);
}

void Signal::drive(Level lvl) {
    Level old = pending_.load(std::memory_order_acquire);
    if (lvl == old) return;
    pending_.store(lvl, std::memory_order_release);
    if (!dirty_.exchange(true, std::memory_order_relaxed))
        scheduler_->mark_dirty(this);
}

void Signal::release() {
    drive(pull_);
}

void Signal::reset() {
    level_.store(Level::HiZ, std::memory_order_release);
    pending_.store(Level::HiZ, std::memory_order_release);
    dirty_.store(false, std::memory_order_relaxed);
}

void Signal::set_pull(Level pull) {
    pull_ = pull;
    if (level_.load(std::memory_order_acquire) == Level::HiZ && pull != Level::HiZ) {
        drive(pull);
    }
}

bool Signal::commit() {
    Level p = pending_.load(std::memory_order_acquire);
    Level c = level_.load(std::memory_order_acquire);
    if (p == c) return false;
    level_.store(p, std::memory_order_release);
    return true;
}

void Signal::connect(Component* c) {
    c->subscribe_to(*this);
}

void Signal::disconnect(Component* c) {
    // Only async subscribers need disconnect for now.
    // InlineComponents don't disconnect individually.
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
