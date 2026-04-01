#pragma once
// PC speaker audio -- PIT-sampled ring buffer approach (a la MartyPC).
//
// The PIT (IC_8253) samples channel 2 OUT every PIT tick (~1.193 MHz),
// ANDs it with PB1 (speaker output enable from IC_8255A), and box-averages
// every 25 samples into one float pushed to an SPSC ring buffer at ~47.7 kHz.
//
// The audio callback (miniaudio, 48 kHz) pops from the ring and plays.
// On underrun it holds the last sample.  This captures every PIT output
// transition faithfully, including mode 0 PWM "realsound" digitized speech.
//
// Usage:
//   PCSpeaker spk;
//   spk.init();
//   pit->set_speaker(&spk);   // PIT pushes decimated samples
//   ppi->set_speaker(&spk);   // PPI writes pit_output_enabled
//   // ... run sim ...
//   spk.shutdown();

#include <atomic>
#include <cstdint>

namespace bench {

// PIT ticks per speaker sample: 1,193,182 / 25 = 47,727 Hz.
static constexpr uint32_t SPEAKER_SAMPLE_RATIO = 25;
static constexpr uint32_t SPEAKER_SAMPLE_RATE  = 47727;

// Lock-free SPSC ring buffer (sim thread produces, audio callback consumes).
// Follows the same pattern as floppy::SeekQueue.
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

// Shared state written by IC_8255A (sim thread), read by IC_8253 (sim thread).
// Both run on the same sim thread so no synchronization needed.
struct SpeakerParams {
    bool pit_output_enabled = false;  // PB1: speaker output enable
};

class PCSpeaker {
public:
    PCSpeaker() = default;
    ~PCSpeaker();

    PCSpeaker(const PCSpeaker&) = delete;
    PCSpeaker& operator=(const PCSpeaker&) = delete;

    bool init(uint32_t sample_rate = SPEAKER_SAMPLE_RATE);
    void shutdown();

    // Called from IC_8253 at ~47.7 kHz (every 25 PIT ticks) with a
    // box-averaged sample in [0.0, 1.0].
    void push_sample(float sample) { ring_.push(sample); }

    // Shared params -- IC_8255A writes pit_output_enabled here.
    SpeakerParams& params() { return params_; }
    const SpeakerParams& params() const { return params_; }

    struct Impl;
private:
    Impl* impl_ = nullptr;
    SpeakerParams params_;
    SpeakerRing ring_;
};

} // namespace bench
