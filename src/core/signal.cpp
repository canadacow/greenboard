#include "core/signal.h"
#include "core/component.h"
#include "core/inline_component.h"
#include "core/scheduler.h"
#include <cassert>

namespace bench {

// --- SignalPool ---

alignas(64) Level SignalPool::current[MAX_SIGNALS] = {};
alignas(64) Level SignalPool::pending[MAX_SIGNALS] = {};
int SignalPool::count = 1;  // slot 0 reserved as dummy (reads HiZ, writes vanish)

// --- Signal ---

std::atomic<int> Signal::pending_count{0};
Scheduler* Signal::scheduler_ = nullptr;

Signal::Signal(std::string name) : name_(std::move(name)) {
    int idx = SignalPool::allocate();
    assert(idx < SignalPool::MAX_SIGNALS && "Signal pool exhausted");
    current_ = &SignalPool::current[idx];
    pending_ = &SignalPool::pending[idx];
    *current_ = Level::HiZ;
    *pending_ = Level::HiZ;
}

void Signal::set_pull(Level pull) {
    pull_ = pull;
    if (*current_ == Level::HiZ && pull != Level::HiZ) {
        drive(pull);
    }
}

void Signal::reset() {
    *current_ = Level::HiZ;
    *pending_ = Level::HiZ;
}

void Signal::connect(Component* c) {
    c->subscribe_to(*this);
}

void Signal::disconnect(Component* c) {
    // Only async subscribers need disconnect for now.
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
