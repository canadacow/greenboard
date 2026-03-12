#pragma once
#include "core/signal.h"
#include "core/mailbox.h"
#include "core/inline_component.h"
#include <atomic>
#include <cassert>

namespace bench {

// Central synchronous evaluator for inline (combinational) ICs.
//
// Called by the 8284A at each CLK edge. Commits dirty signals,
// evaluates inline ICs to fixed-point, wakes all registered async
// components, and waits for quiescence.
//
// Wake-all model: every evaluate() wakes every registered async
// component unconditionally. No per-signal subscriber tracking.
//
// Thread safety: mark_dirty() is MPSC-safe (multiple producers via
// atomic fetch_add). evaluate() must be called from a single thread.
class Scheduler {
public:
    void register_inline(InlineComponent* ic) {
        assert(inline_count_ < MAX_INLINES && "Scheduler: too many inline ICs");
        inlines_[inline_count_++] = ic;
    }

    void register_async(Mailbox* mb) {
        assert(async_count_ < MAX_ASYNC && "Scheduler: too many async components");
        asyncs_[async_count_++] = mb;
    }

    // Called from Signal::drive() -- MPSC-safe.
    void mark_dirty(Signal* sig) {
        int idx = dirty_count_.fetch_add(1, std::memory_order_relaxed);
        assert(idx < MAX_DIRTY && "Scheduler: dirty list overflow");
        dirty_[idx] = sig;
    }

    // Commit + eval inlines + wake all async. Does NOT wait for
    // quiescence -- the caller (8284A) provides that via its drain loop.
    void evaluate() {
        commit_and_eval_inlines();

        // Wake ALL registered async components.
        if (async_count_ > 0) {
            Signal::pending.fetch_add(async_count_, std::memory_order_release);
            for (int i = 0; i < async_count_; ++i)
                asyncs_[i]->wake();
        }
    }

    // Commit + eval inlines only. No async wake.
    // Used by the main thread (e.g. VCC at startup).
    void evaluate_no_wake() {
        commit_and_eval_inlines();
    }

private:
    void commit_and_eval_inlines() {
        // Phase 1: Commit all dirty signals.
        int n_dirty = dirty_count_.load(std::memory_order_relaxed);

        for (int i = 0; i < n_dirty; ++i) {
            Signal* sig = dirty_[i];
            sig->dirty_.store(false, std::memory_order_relaxed);
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
                sig->dirty_.store(false, std::memory_order_relaxed);
                if (sig->commit())
                    any_changed = true;
            }
            dirty_count_.store(0, std::memory_order_relaxed);

            if (!any_changed) break;
        }
    }

    static constexpr int MAX_INLINES = 32;
    static constexpr int MAX_DIRTY = 512;
    static constexpr int MAX_ASYNC = 32;

    InlineComponent* inlines_[MAX_INLINES] = {};
    int inline_count_ = 0;

    Mailbox* asyncs_[MAX_ASYNC] = {};
    int async_count_ = 0;

    Signal* dirty_[MAX_DIRTY] = {};
    std::atomic<int> dirty_count_{0};
};

} // namespace bench
