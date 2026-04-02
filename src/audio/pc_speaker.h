#pragma once
// PC speaker audio -- raw digital ring buffer + miniaudio playback.
//
// SpeakerDriver pushes raw 0/1 digital state at system CLK rate (~4.77 MHz)
// into a large SPSC ring buffer.  The audio callback consumes 100 entries per
// output sample (47,727 Hz), applying the speaker coil L/R lowpass filter.
// All analog modeling happens in the audio thread -- zero float math in the DAG.
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

// System CLK cycles per audio sample: 4,772,727 / 100 = 47,727 Hz.
static constexpr uint32_t SPEAKER_SAMPLE_RATE  = 47727;
static constexpr uint32_t CLK_PER_AUDIO_SAMPLE = 100;

// Lock-free SPSC ring buffer of raw digital speaker state.
// Each entry is one system CLK cycle: 0 = speaker idle, 1 = speaker driven.
// 524288 entries at 4.77 MHz = ~110 ms of buffer.
struct SpeakerRing {
    static constexpr uint32_t SIZE = 524288;  // must be power of 2

    uint8_t* buf = nullptr;
    alignas(64) std::atomic<uint32_t> write{0};
    alignas(64) std::atomic<uint32_t> read{0};

    SpeakerRing()  { buf = new uint8_t[SIZE](); }
    ~SpeakerRing() { delete[] buf; }

    SpeakerRing(const SpeakerRing&) = delete;
    SpeakerRing& operator=(const SpeakerRing&) = delete;

    void push(uint8_t val) {
        uint32_t w = write.load(std::memory_order_relaxed);
        // No overflow check -- if sim outruns audio, oldest samples
        // are silently overwritten.  The audio thread will catch up.
        buf[w & (SIZE - 1)] = val;
        write.store(w + 1, std::memory_order_release);
    }

    bool pop(uint8_t& val) {
        uint32_t r = read.load(std::memory_order_relaxed);
        uint32_t w = write.load(std::memory_order_acquire);
        if (r == w) return false;
        val = buf[r & (SIZE - 1)];
        read.store(r + 1, std::memory_order_release);
        return true;
    }

    uint32_t available() const {
        return write.load(std::memory_order_acquire) -
               read.load(std::memory_order_relaxed);
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

    // Called from SpeakerDriver every system CLK cycle.
    // val: 1 = speaker driven, 0 = idle.
    void push(uint8_t val) { ring_.push(val); }

    struct Impl;
private:
    Impl* impl_ = nullptr;
    SpeakerRing ring_;
};

} // namespace bench
