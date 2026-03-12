#pragma once
#include "core/signal.h"
#include "core/fiber_component.h"
#include "core/inline_component.h"
#include <atomic>
#include <cassert>
#include <spdlog/spdlog.h>

namespace bench {

// Central synchronous evaluator -- the beating heart of the simulation.
//
// Called by the 8284A at each CLK edge. Commits dirty signals,
// evaluates inline ICs to fixed-point, then resumes all fiber
// components (one at a time, cooperatively).
//
// Fiber model: every registered FiberComponent is resumed once per
// evaluate() call. Each fiber runs until it yields, then the next
// fiber is resumed. Completely deterministic, single-threaded,
// zero synchronization overhead.
//
// Thread safety: mark_dirty() is safe to call from the 8284A's thread
// (the only thread). evaluate() runs on that same thread.
class Scheduler {
public:
    void register_inline(InlineComponent* ic) {
        assert(inline_count_ < MAX_INLINES && "Scheduler: too many inline ICs");
        inlines_[inline_count_++] = ic;
    }

    void register_fiber(FiberComponent* fc) {
        assert(fiber_count_ < MAX_FIBERS && "Scheduler: too many fiber components");
        fibers_[fiber_count_++] = fc;
    }

    // Called from Signal::drive().
    void mark_dirty(Signal* sig) {
        int idx = dirty_count_.fetch_add(1, std::memory_order_relaxed);
        assert(idx < MAX_DIRTY && "Scheduler: dirty list overflow");
        dirty_[idx] = sig;
    }

    // Commit + eval inlines + resume all fibers.
    // caller = the 8284A's fiber handle (from fiber_convert_thread).
    void evaluate(Fiber caller) {
        commit_and_eval_inlines();

        // Resume ALL registered fiber components.
        for (int i = 0; i < fiber_count_; ++i)
            fibers_[i]->resume(caller);
    }

    // Commit + eval inlines only. No fiber resume.
    // Used by the main thread (e.g. VCC at startup, shutdown).
    void evaluate_no_wake() {
        commit_and_eval_inlines();
    }

private:
    void commit_and_eval_inlines() {
        // Phase 1: Commit all dirty signals.
        int n_dirty = dirty_count_.load(std::memory_order_relaxed);

        for (int i = 0; i < n_dirty; ++i) {
            Signal* sig = dirty_[i];
            sig->dirty_ = false;
            sig->commit();
        }
        dirty_count_.store(0, std::memory_order_relaxed);

        // Phase 2: Fixed-point inline IC evaluation.
        for (;;) {
            for (int i = 0; i < inline_count_; ++i) {
                if (inlines_[i]->is_powered())
                    inlines_[i]->on_signal_change();
            }

            int new_dirty = dirty_count_.load(std::memory_order_relaxed);
            if (new_dirty == 0) break;

            bool any_changed = false;
            for (int i = 0; i < new_dirty; ++i) {
                Signal* sig = dirty_[i];
                sig->dirty_ = false;
                if (sig->commit())
                    any_changed = true;
            }
            dirty_count_.store(0, std::memory_order_relaxed);

            if (!any_changed) break;
        }
    }

    static constexpr int MAX_INLINES = 32;
    static constexpr int MAX_DIRTY = 512;
    static constexpr int MAX_FIBERS = 256;

    InlineComponent* inlines_[MAX_INLINES] = {};
    int inline_count_ = 0;

    FiberComponent* fibers_[MAX_FIBERS] = {};
    int fiber_count_ = 0;

    Signal* dirty_[MAX_DIRTY] = {};
    std::atomic<int> dirty_count_{0};
};

} // namespace bench
