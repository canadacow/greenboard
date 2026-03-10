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

// Wake-only mailbox. No queue -- just a semaphore to wake the consumer
// when any connected signal changes. Components poll signal levels
// directly when woken.
struct alignas(64) Mailbox {
    std::counting_semaphore<256>  sem{0};

    // Wake the consumer unconditionally. Called from any thread.
    void wake() {
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
