// PC speaker audio -- ring buffer consumer on a miniaudio thread.
//
// The sim thread (IC_8253) pushes box-averaged speaker samples at ~47.7 kHz
// into an SPSC ring buffer.  This audio callback pops them at the same rate
// and plays through the system audio device.
//
// The audio device runs at 47727 Hz -- an exact integer divisor of the PIT
// clock (1,193,182 / 25), so production and consumption rates match perfectly
// when the sim runs at real-time speed.

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "audio/pc_speaker.h"
#include <cstring>
#include <cstdio>

namespace bench {

// Output amplitude.  The real PC speaker is not subtle.
static constexpr float SPKR_VOLUME = 0.45f;

// One-pole lowpass coefficient (~8 kHz at 47727 Hz sample rate).
// Softens the raw square wave edges, models speaker cone inertia.
static constexpr float LP_ALPHA = 0.65f;

// Pre-fill: don't start consuming until the ring has this many samples.
// ~2048 samples at 47727 Hz = ~43 ms of latency cushion.
static constexpr uint32_t PREFILL_SAMPLES = 2048;

// Fade rate toward silence on underrun (per sample).
// Avoids the hard discontinuity when data resumes.
static constexpr float FADE_RATE = 0.002f;

struct PCSpeaker::Impl {
    ma_device   device;
    bool        device_inited = false;
    uint32_t    sample_rate   = SPEAKER_SAMPLE_RATE;

    SpeakerRing* ring = nullptr;

    // Audio-thread-local state.
    float last_sample = 0.0f;   // last popped value
    float lp_state    = 0.0f;   // lowpass filter state
    bool  primed      = false;  // ring has filled past PREFILL_SAMPLES at least once
};

// -----------------------------------------------------------------------
// Audio callback -- ring buffer consumer with pre-fill gate
// -----------------------------------------------------------------------

static void audio_callback(ma_device* device, void* output, const void* /*input*/,
                           ma_uint32 frame_count)
{
    auto* impl = static_cast<PCSpeaker::Impl*>(device->pUserData);
    auto* out  = static_cast<float*>(output);

    // Wait for the ring to accumulate a cushion before we start playing.
    // This absorbs jitter from the sim thread's startup burst.
    if (!impl->primed) {
        uint32_t w = impl->ring->write.load(std::memory_order_acquire);
        uint32_t r = impl->ring->read.load(std::memory_order_relaxed);
        if (w - r < PREFILL_SAMPLES) {
            std::memset(out, 0, frame_count * sizeof(float));
            return;
        }
        impl->primed = true;
    }

    for (ma_uint32 i = 0; i < frame_count; ++i) {
        float sample;
        if (impl->ring->pop(sample)) {
            impl->last_sample = sample;
        } else {
            // Underrun: fade toward silence instead of holding.
            if (impl->last_sample > FADE_RATE)
                impl->last_sample -= FADE_RATE;
            else if (impl->last_sample < -FADE_RATE)
                impl->last_sample += FADE_RATE;
            else
                impl->last_sample = 0.0f;
            sample = impl->last_sample;
        }

        // Scale and lowpass.
        float scaled = sample * SPKR_VOLUME;
        impl->lp_state += LP_ALPHA * (scaled - impl->lp_state);
        out[i] = impl->lp_state;
    }
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

PCSpeaker::~PCSpeaker() { shutdown(); }

bool PCSpeaker::init(uint32_t sample_rate /* = SPEAKER_SAMPLE_RATE */) {
    if (impl_) return false;

    impl_ = new Impl;
    impl_->sample_rate = sample_rate;
    impl_->ring        = &ring_;

    ma_device_config cfg   = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format    = ma_format_f32;
    cfg.playback.channels  = 1;
    cfg.sampleRate         = sample_rate;
    cfg.dataCallback       = audio_callback;
    cfg.pUserData          = impl_;
    cfg.periodSizeInFrames = 1024;   // ~21 ms per period at 47727 Hz
    cfg.periods            = 3;      // triple-buffer for jitter tolerance

    if (ma_device_init(nullptr, &cfg, &impl_->device) != MA_SUCCESS) {
        std::printf("[PCSpeaker] ERROR: ma_device_init failed\n");
        delete impl_;
        impl_ = nullptr;
        return false;
    }
    impl_->device_inited = true;

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        std::printf("[PCSpeaker] ERROR: ma_device_start failed\n");
        ma_device_uninit(&impl_->device);
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    std::printf("[PCSpeaker] audio initialized (%u Hz, mono, period=1024x3, prefill=%u)\n",
                sample_rate, PREFILL_SAMPLES);
    return true;
}

void PCSpeaker::shutdown() {
    if (!impl_) return;
    if (impl_->device_inited) {
        ma_device_stop(&impl_->device);
        ma_device_uninit(&impl_->device);
    }
    delete impl_;
    impl_ = nullptr;
}

} // namespace bench
