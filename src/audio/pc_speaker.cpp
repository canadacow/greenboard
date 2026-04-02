// PC speaker audio -- consumes raw digital samples, applies analog modeling.
//
// The ring buffer contains one 0/1 per system CLK cycle (~4.77 MHz).
// Each audio output sample (47,727 Hz) consumes 100 ring entries.
// The speaker coil L/R lowpass is applied here, not in the DAG.
//
// Speaker analog model (BRD-verified):
//   75477 Darlington driver -> C9 (.01uF) -> R10 (33 ohm) -> 8 ohm speaker
//   Speaker inductance ~1 mH, total R = 41 ohm.
//   tau = L/R = 1e-3 / 41 = 24.39 us.
//   At system CLK rate (dt = 0.2095 us):
//     alpha = dt / (tau + dt) = 0.2095 / 24.60 = 0.00852.

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "audio/pc_speaker.h"
#include <cstring>
#include <cstdio>

namespace bench {

static constexpr float SPKR_VOLUME = 0.45f;

// Speaker coil lowpass: L/R filter applied per CLK-rate sample.
static constexpr float LP_ALPHA = 0.00852f;

struct PCSpeaker::Impl {
    ma_device   device;
    bool        device_inited = false;
    uint32_t    sample_rate   = SPEAKER_SAMPLE_RATE;

    SpeakerRing* ring = nullptr;

    float coil_state  = 0.0f;   // lowpass filter state (speaker coil current)
    float last_output = 0.0f;   // last produced audio sample (for underrun hold)
};

// -----------------------------------------------------------------------
// Audio callback -- analog modeling happens here
// -----------------------------------------------------------------------

static void audio_callback(ma_device* device, void* output, const void* /*input*/,
                           ma_uint32 frame_count)
{
    auto* impl = static_cast<PCSpeaker::Impl*>(device->pUserData);
    auto* out  = static_cast<float*>(output);

    for (ma_uint32 i = 0; i < frame_count; ++i) {
        // Consume CLK_PER_AUDIO_SAMPLE (100) digital samples from the ring.
        // Apply the speaker coil lowpass to each one.
        uint8_t val;
        uint32_t consumed = 0;
        while (consumed < CLK_PER_AUDIO_SAMPLE && impl->ring->pop(val)) {
            float target = val ? 1.0f : 0.0f;
            impl->coil_state += LP_ALPHA * (target - impl->coil_state);
            ++consumed;
        }

        // If we got samples, use the final filtered value.
        // If ring was empty (debugger pause), hold last output.
        if (consumed > 0)
            impl->last_output = impl->coil_state * SPKR_VOLUME;

        out[i] = impl->last_output;
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

    std::printf("[PCSpeaker] audio initialized (%u Hz, ring=%uK raw CLK samples)\n",
                sample_rate, SpeakerRing::SIZE / 1024);
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
