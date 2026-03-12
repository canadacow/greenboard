#pragma once
#include "core/signal.h"
#include "core/mailbox.h"
#include "core/inline_component.h"
#include <atomic>
#include <thread>
#include <cassert>
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace bench {

// Central clock: commits signals, evaluates inline ICs, wakes all
// async components, waits for quiescence, repeats.
//
// One tick = one evaluation cycle. Every registered async component
// is woken unconditionally each tick. No per-signal subscriber
// tracking -- pending count is deterministic.
//
// Thread safety: mark_dirty() is MPSC-safe (multiple producers via
// atomic fetch_add). The run loop is the sole consumer.
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

    // Start the scheduler thread.
    void start() {
        if (thread_.joinable()) return;
        thread_ = std::jthread([this](std::stop_token stop) {
#ifdef _WIN32
            SetThreadDescription(GetCurrentThread(), L"Scheduler");
#endif
            run(stop);
        });
        spdlog::debug("[Scheduler] started");
    }

    // Stop the scheduler thread.
    void stop() {
        if (!thread_.joinable()) return;
        thread_.request_stop();
        thread_.join();
        spdlog::debug("[Scheduler] stopped");
    }

    // Single-shot evaluate for use by the main thread (e.g. driving
    // VCC at startup). Does NOT wake async components or wait for
    // quiescence -- just commits dirty signals and runs inline ICs.
    void evaluate() {
        commit_and_eval_inlines();
    }

private:
    void run(std::stop_token stop) {
        while (!stop.stop_requested()) {
            // Commit + inline fixed-point.
            commit_and_eval_inlines();

            // Wake ALL registered async components, wait for quiescence.
            if (async_count_ > 0) {
                Signal::pending.fetch_add(async_count_, std::memory_order_release);
                for (int i = 0; i < async_count_; ++i)
                    asyncs_[i]->wake();

                while (Signal::pending.load(std::memory_order_acquire) > 0
                       && !stop.stop_requested())
                    ;
            }
        }
    }

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

    std::jthread thread_;
};

} // namespace bench
