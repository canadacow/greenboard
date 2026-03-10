#pragma once
#include "core/types.h"
#include <atomic>
#include <semaphore>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <vector>

namespace bench {

class Signal;

// A signal change event delivered to a component's mailbox.
struct SignalEvent {
    Signal* signal;
    Level old_level;
    Level new_level;
};

// Lock-free MPSC ring buffer for signal events.
//
// Allocated from a static pool so its memory remains valid after the
// owning Component is destroyed. Signals hold Mailbox* and post() is
// always safe -- writes go to valid memory even if nobody reads.
struct alignas(64) Mailbox {
    static constexpr uint32_t kCapacity = 256;  // must be power of 2

    SignalEvent ring[kCapacity] = {};
    alignas(64) std::atomic<uint32_t> tail{0};     // next write slot (producers)
    alignas(64) uint32_t              head{0};      // next read slot (consumer only)
    alignas(64) std::atomic<uint32_t> committed{0}; // slots fully written
    alignas(64) std::atomic<bool>     sleeping{false};
    std::binary_semaphore             sem{0};

    // Thread-safe: may be called from any thread (lock-free).
    void post(const SignalEvent& event) {
        uint32_t slot = tail.fetch_add(1, std::memory_order_acq_rel);
        ring[slot & (kCapacity - 1)] = event;
        committed.fetch_add(1, std::memory_order_release);
        if (sleeping.load(std::memory_order_acquire))
            sem.release();
    }

    // Allocate a Mailbox from the static pool. Lives until program exit.
    static Mailbox* allocate() {
        static constexpr size_t kMax = 128;
        static constexpr size_t kAlign = alignof(Mailbox);
        static constexpr size_t kSlotSize =
            (sizeof(Mailbox) + kAlign - 1) & ~(kAlign - 1);

        // One-time allocation of raw backing store.
        static std::vector<uint8_t> pool(kMax * kSlotSize);
        static size_t count = 0;

        if (count >= kMax) std::abort();
        uint8_t* slot = pool.data() + count * kSlotSize;
        ++count;
        return new (slot) Mailbox();
    }
};

} // namespace bench
