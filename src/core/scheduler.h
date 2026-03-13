#pragma once
#include "core/signal.h"
#include "core/callback_component.h"
#include "core/fiber_component.h"
#include "core/inline_component.h"
#include <cassert>
#include <spdlog/spdlog.h>

namespace bench {

// Central synchronous evaluator -- the beating heart of the simulation.
//
// Called by the 8284A at each CLK edge. Commits all signals via
// SignalPool::commit(), evaluates inline ICs to fixed-point,
// then resumes all fiber components cooperatively.
//
// No per-signal bookkeeping. drive() is a single comparison + store.
// Commit is a tight loop over two contiguous arrays.
class Scheduler {
public:
    void register_inline(InlineComponent* ic) {
        assert(inline_count_ < MAX_INLINES && "Scheduler: too many inline ICs");
        inlines_[inline_count_++] = ic;
    }

    void register_callback(CallbackComponent* cc) {
        assert(callback_count_ < MAX_CALLBACKS && "Scheduler: too many callback ICs");
        callbacks_[callback_count_++] = cc;
    }

    void register_fiber(FiberComponent* fc) {
        assert(fiber_count_ < MAX_FIBERS && "Scheduler: too many fiber components");
        fibers_[fiber_count_++] = fc;
    }

    // Half-cycle flag: set by the 8088 before yielding.
    // The 8284A checks this after evaluate() returns.
    bool half_cycle_requested() const { return half_cycle_; }
    void request_half_cycle() { half_cycle_ = true; }
    void clear_half_cycle() { half_cycle_ = false; }

    // Commit + callbacks + inlines (fixed-point) + fibers.
    // Order: commit pending signals, run callbacks (sequential ICs like
    // 8288 that advance state once per eval), commit their outputs,
    // then run inlines to fixed-point (combinational ripple), then fibers.
    void evaluate(Fiber caller, bool rising = true, bool falling = true) {
        evaluate_no_wake(rising, falling);
        for (int i = 0; i < fiber_count_; ++i)
            fibers_[i]->resume(caller);
    }

    // Commit + callbacks + inlines only. No fiber resume.
    void evaluate_no_wake(bool rising = true, bool falling = true) {
        SignalPool::commit();
        for (int i = 0; i < callback_count_; ++i)
            callbacks_[i]->on_signal_change(rising, falling);
        commit_and_eval_inlines(rising, falling);
    }

private:
    void commit_and_eval_inlines(bool rising = true, bool falling = true) {
        // Phase 1: Commit all signals.
        if (!SignalPool::commit()) return;

        // Phase 2: Fixed-point inline IC evaluation.
        // No is_powered() check -- inlines are always powered during eval.
        for (;;)
        {
            for (int i = 0; i < inline_count_; ++i)
                inlines_[i]->on_signal_change(rising, falling);

            if (!SignalPool::commit()) break;
        }
    }

    bool half_cycle_ = false;

    static constexpr int MAX_INLINES = 32;
    static constexpr int MAX_CALLBACKS = 32;
    static constexpr int MAX_FIBERS = 256;

    InlineComponent* inlines_[MAX_INLINES] = {};
    int inline_count_ = 0;

    CallbackComponent* callbacks_[MAX_CALLBACKS] = {};
    int callback_count_ = 0;

    FiberComponent* fibers_[MAX_FIBERS] = {};
    int fiber_count_ = 0;
};

} // namespace bench
