#pragma once
#include "core/signal.h"
#include "core/inline_component.h"
#include <atomic>
#include <cassert>

namespace bench {

// Fixed-capacity set on the stack. Linear scan -- fine for N < ~512.
template<typename T, int Capacity>
class StackSet {
public:
    bool insert(T val) {
        for (int i = 0; i < count_; ++i)
            if (items_[i] == val) return false;
        assert(count_ < Capacity && "StackSet overflow");
        items_[count_++] = val;
        return true;
    }
    int   count() const     { return count_; }
    T     operator[](int i) const { return items_[i]; }
private:
    T   items_[Capacity] = {};
    int count_ = 0;
};

// Central synchronous evaluator for inline (combinational) ICs.
//
// Double-buffered signal model:
//   - drive() writes to Signal::pending_ (never directly to level_)
//   - level() reads from Signal::level_ (committed value)
//   - evaluate() commits pending -> level, evaluates inline ICs to
//     fixed-point, and wakes async subscribers on changed outputs.
//
// Called by the 8284A (or test harness) at each CLK edge, after all
// threaded ICs have settled (wait_quiescent).
//
// Thread safety: mark_dirty() is MPSC-safe (multiple producers via
// atomic fetch_add). evaluate() must be called from a single thread
// after quiescence.
class Scheduler {
public:
    void register_inline(InlineComponent* ic) {
        assert(inline_count_ < MAX_INLINES && "Scheduler: too many inline ICs");
        inlines_[inline_count_++] = ic;
    }

    // Called from Signal::drive() -- MPSC-safe.
    void mark_dirty(Signal* sig) {
        int idx = dirty_count_.fetch_add(1, std::memory_order_relaxed);
        assert(idx < MAX_DIRTY && "Scheduler: dirty list overflow");
        dirty_[idx] = sig;
    }

    // Commit pending values, evaluate inline ICs to fixed-point, wake.
    void evaluate() {
        // Phase 1: Commit all dirty signals, track which actually changed.
        int n_dirty = dirty_count_.load(std::memory_order_relaxed);
        StackSet<Signal*, MAX_DIRTY> changed;

        for (int i = 0; i < n_dirty; ++i) {
            Signal* sig = dirty_[i];
            sig->dirty_.store(false, std::memory_order_relaxed);
            if (sig->commit())
                changed.insert(sig);
        }
        dirty_count_.store(0, std::memory_order_relaxed);

        // Phase 2: Fixed-point inline IC evaluation.
        for (;;) {
            for (int i = 0; i < inline_count_; ++i) {
                if (inlines_[i]->is_powered())
                    inlines_[i]->on_signal_change();
            }

            // Commit any new dirty signals from inline IC outputs.
            int new_dirty = dirty_count_.load(std::memory_order_relaxed);
            if (new_dirty == 0) break;  // Fixed point reached.

            bool any_changed = false;
            for (int i = 0; i < new_dirty; ++i) {
                Signal* sig = dirty_[i];
                sig->dirty_.store(false, std::memory_order_relaxed);
                if (sig->commit()) {
                    changed.insert(sig);
                    any_changed = true;
                }
            }
            dirty_count_.store(0, std::memory_order_relaxed);

            if (!any_changed) break;  // All settled.
        }

        // Phase 3: Wake each unique mailbox exactly once.
        StackSet<Mailbox*, MAX_SUBS> wakes;
        for (int i = 0; i < changed.count(); ++i) {
            for (auto* mb : changed[i]->subscribers_)
                wakes.insert(mb);
        }
        Signal::pending.fetch_add(wakes.count(), std::memory_order_release);
        for (int i = 0; i < wakes.count(); ++i)
            wakes[i]->wake();
    }

private:
    static constexpr int MAX_INLINES = 32;
    static constexpr int MAX_DIRTY = 512;
    static constexpr int MAX_SUBS = 128;

    InlineComponent* inlines_[MAX_INLINES] = {};
    int inline_count_ = 0;

    Signal* dirty_[MAX_DIRTY] = {};
    std::atomic<int> dirty_count_{0};
};

} // namespace bench
