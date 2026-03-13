#pragma once
#include "core/signal.h"
#include "core/bus_controller_component.h"
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

    void register_bus_controller(BusControllerComponent* bc) {
        assert(bus_ctrl_count_ < MAX_BUS_CTRLS && "Scheduler: too many bus controllers");
        bus_ctrls_[bus_ctrl_count_++] = bc;
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

    // Commit + bus controllers + settle inlines + callbacks. No fiber resume.
    //
    // Order matches real hardware propagation within a clock period:
    //   1. Commit pending signals (8088 status lines become visible)
    //   2. Bus controllers (8288 decodes S0-S2, drives ~IOW/~MEMR/ALE/etc.)
    //      -- 8288 uses drive_immediate() so outputs are in current[] already
    //   3. Commit + settle inlines (74S373 latches address, 74S138 decodes ~CS)
    //   4. Regular callbacks (8259A, 8253, etc. see fully settled bus)
    void evaluate_no_wake(bool rising = true, bool falling = true) {
        SignalPool::commit();
        for (int i = 0; i < bus_ctrl_count_; ++i)
            bus_ctrls_[i]->on_signal_change(rising, falling);

        // Phase 2: Fixed-point inline IC evaluation.
        // No is_powered() check -- inlines are always powered during eval.
        for (;;)
        {
            for (int i = 0; i < inline_count_; ++i)
                inlines_[i]->on_signal_change(rising, falling);

            if (!SignalPool::commit()) break;
        }

        for (int i = 0; i < callback_count_; ++i)
            callbacks_[i]->on_signal_change(rising, falling);
    }

private:
    bool half_cycle_ = false;

    static constexpr int MAX_BUS_CTRLS = 4;
    static constexpr int MAX_INLINES = 32;
    static constexpr int MAX_CALLBACKS = 32;
    static constexpr int MAX_FIBERS = 256;

    BusControllerComponent* bus_ctrls_[MAX_BUS_CTRLS] = {};
    int bus_ctrl_count_ = 0;

    InlineComponent* inlines_[MAX_INLINES] = {};
    int inline_count_ = 0;

    CallbackComponent* callbacks_[MAX_CALLBACKS] = {};
    int callback_count_ = 0;

    FiberComponent* fibers_[MAX_FIBERS] = {};
    int fiber_count_ = 0;
};

} // namespace bench
