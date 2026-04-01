#pragma once
// PC speaker audio -- SPSC ring buffer + miniaudio playback.
//
// SpeakerDriver (board-level component) pushes decimated samples at ~47.7 kHz
// into the ring.  The audio callback pops and plays at the same rate.
//
// Usage:
//   PCSpeaker spk;
//   spk.init();
//   speaker_driver.connect(board.spkr_mix, &spk);
//   // ... run sim ...
//   spk.shutdown();

#include <atomic>
#include <cstdint>

namespace bench {

// System CLK cycles per speaker sample: 4,772,727 / 100 = 47,727 Hz.
static constexpr uint32_t SPEAKER_SAMPLE_RATE = 47727;

// Lock-free SPSC ring buffer (sim thread produces, audio callback consumes).
struct SpeakerRing {
    static constexpr uint32_t SIZE = 16384;  // ~340 ms at 47.7 kHz

    float buf[SIZE] = {};
    alignas(64) std::atomic<uint32_t> write{0};
    alignas(64) std::atomic<uint32_t> read{0};

    bool push(float val) {
        uint32_t w = write.load(std::memory_order_relaxed);
        uint32_t r = read.load(std::memory_order_acquire);
        if (w - r >= SIZE) return false;
        buf[w & (SIZE - 1)] = val;
        write.store(w + 1, std::memory_order_release);
        return true;
    }

    bool pop(float& val) {
        uint32_t r = read.load(std::memory_order_relaxed);
        uint32_t w = write.load(std::memory_order_acquire);
        if (r == w) return false;
        val = buf[r & (SIZE - 1)];
        read.store(r + 1, std::memory_order_release);
        return true;
    }
};

class PCSpeaker {
public:
    PCSpeaker() = default;
    ~PCSpeaker();

    PCSpeaker(const PCSpeaker&) = delete;
    PCSpeaker& operator=(const PCSpeaker&) = delete;

    bool init(uint32_t sample_rate = SPEAKER_SAMPLE_RATE);
    void shutdown();

    // Called from SpeakerDriver at ~47.7 kHz with a box-averaged
    // voltage sample in [0.0, 1.0] (post MC1741/75477 modeling).
    void push_sample(float sample) { ring_.push(sample); }

    struct Impl;
private:
    Impl* impl_ = nullptr;
    SpeakerRing ring_;
};

} // namespace bench
