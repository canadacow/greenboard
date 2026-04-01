// PC speaker audio -- ring buffer consumer on a miniaudio thread.
//
// SpeakerDriver pushes samples at ~47.7 kHz into an SPSC ring buffer.
// The audio callback pops and plays at the same rate.
//
// The sim runs faster than real-time, so the ring tends to fill.
// The audio callback is the pacing clock -- it drains at 47727 Hz and
// the ring absorbs the sim's burst-ahead.  On underrun (debugger pause,
// sim stall) the callback outputs silence.

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "audio/pc_speaker.h"
#include <cstring>
#include <cstdio>

namespace bench {

// Output amplitude.
static constexpr float SPKR_VOLUME = 0.45f;

struct PCSpeaker::Impl {
    ma_device   device;
    bool        device_inited = false;
    uint32_t    sample_rate   = SPEAKER_SAMPLE_RATE;

    SpeakerRing* ring = nullptr;

    // Audio-thread-local state.
    float last_sample = 0.0f;
};

// -----------------------------------------------------------------------
// Audio callback
// -----------------------------------------------------------------------

static void audio_callback(ma_device* device, void* output, const void* /*input*/,
                           ma_uint32 frame_count)
{
    auto* impl = static_cast<PCSpeaker::Impl*>(device->pUserData);
    auto* out  = static_cast<float*>(output);

    for (ma_uint32 i = 0; i < frame_count; ++i) {
        float sample;
        if (impl->ring->pop(sample)) {
            impl->last_sample = sample;
        } else {
            // Underrun (debugger pause, sim stall): hold last value.
            sample = impl->last_sample;
        }
        out[i] = sample * SPKR_VOLUME;
    }
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

PCSpeaker::~PCSpeaker() { shutdown(); }

bool PCSpeaker::init(uint32_t sample_rate) {
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
    cfg.periodSizeInFrames = 1024;
    cfg.periods            = 3;

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

    std::printf("[PCSpeaker] audio initialized (%u Hz, mono, period=1024x3)\n", sample_rate);
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
