// PC speaker audio -- waveform synthesis on a miniaudio audio thread.
//
// Synthesis follows the dosbox-x pcspeaker.cpp approach:
//   - Audio callback runs its own PIT phase accumulator
//   - Reads current PIT mode / reload / gate / output-enable from shared state
//   - Generates the waveform in its own time domain (48 kHz)
//   - Volume ramping smooths transitions (speaker cone inertia)
//
// PIT modes implemented (per 8253 datasheet + dosbox-x):
//   Mode 0: Interrupt on terminal count ("realsound" PWM)
//   Mode 1: Retriggerable one-shot
//   Mode 2: Rate generator (periodic narrow pulse)
//   Mode 3: Square wave generator (most common)
//   Mode 4: Software triggered strobe

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "audio/pc_speaker.h"
#include "ic/ic_8253.h"
#include <cstring>
#include <cmath>
#include <cstdio>

namespace bench {

// PIT channel 2 input clock: 14.31818 MHz / 12 = 1.193182 MHz.
static constexpr double PIT_TICK_RATE = 1193182.0;

// Output amplitude (0.0 - 1.0).  The real PC speaker is not subtle.
static constexpr double SPKR_VOLUME = 0.45;

// Volume ramp: full-scale transition in ~0.15 ms (models speaker cone inertia
// and avoids the harshest digital clicks without attenuating high frequencies).
// Units: amplitude per sample at 48 kHz.
static constexpr double RAMP_PER_SAMPLE_48K = SPKR_VOLUME / 7.0;

// Lowpass coefficient for a simple one-pole RC filter (~8 kHz at 48 kHz).
// Tames the harmonics above Nyquist that a raw square wave would alias.
static constexpr double LP_ALPHA = 0.7;

// -----------------------------------------------------------------------
// Impl -- all audio-thread-local state lives here
// -----------------------------------------------------------------------

struct PCSpeaker::Impl {
    ma_device   device;
    bool        device_inited = false;
    uint32_t    sample_rate   = 48000;

    // Pointers into owning PCSpeaker (stable for device lifetime).
    const IC_8253*    pit    = nullptr;
    SpeakerParams*    params = nullptr;

    // --- Synthesis state (audio thread only) ---

    double phase         = 0.0;   // PIT-tick accumulator within current period
    double phase_inc     = 0.0;   // PIT ticks per audio sample
    bool   mode3_output  = true;  // mode-3 toggle state
    double volcur        = 0.0;   // current output amplitude (ramped)
    double lp_state      = 0.0;   // one-pole lowpass state

    // Cached copies of shared state -- detect changes.
    uint8_t  prev_mode   = 0xFF;
    uint32_t prev_reload = 0;
    bool     prev_gate   = false;

    double ramp_per_sample = RAMP_PER_SAMPLE_48K;
};

// -----------------------------------------------------------------------
// Audio callback
// -----------------------------------------------------------------------

static void audio_callback(ma_device* device, void* output, const void* /*input*/,
                           ma_uint32 frame_count)
{
    auto* impl = static_cast<PCSpeaker::Impl*>(device->pUserData);
    auto* out  = static_cast<float*>(output);

    // --- Snapshot shared state (no sync, CGA pattern) ---
    const bool gate             = impl->params->pit_gate;
    const bool output_enabled   = impl->params->pit_output_enabled;

    uint8_t  pit_mode   = 3;
    uint32_t pit_reload = 0;
    if (impl->pit) {
        pit_mode   = impl->pit->channel2_mode();
        pit_reload = impl->pit->channel2_reload();
    }

    // --- Detect parameter changes ---

    if (pit_mode != impl->prev_mode) {
        impl->phase = 0.0;
        switch (pit_mode) {
            case 0:  impl->mode3_output = false; break;  // OUT starts low
            case 1:  impl->mode3_output = true;  break;  // OUT starts high
            case 2:  impl->mode3_output = true;  break;
            case 3:  impl->mode3_output = true;  break;  // OUT starts high
            case 4:  impl->mode3_output = true;  break;
            default: break;
        }
        impl->prev_mode = pit_mode;
    }

    if (pit_reload != impl->prev_reload) {
        // Counter reprogrammed.  For mode 3, dosbox-x reloads on next
        // half-period boundary; we approximate by resetting phase when
        // the reload changes significantly (>10%) to avoid detuning.
        if (impl->prev_reload > 0) {
            double ratio = (double)pit_reload / (double)impl->prev_reload;
            if (ratio < 0.9 || ratio > 1.1)
                impl->phase = 0.0;
        }
        impl->prev_reload = pit_reload;
    }

    // Gate rising edge: trigger for modes 1 and 3.
    if (gate && !impl->prev_gate) {
        impl->phase = 0.0;
        if (pit_mode == 3) impl->mode3_output = true;
    }
    impl->prev_gate = gate;

    // --- Per-mode constants ---

    const double pit_max  = pit_reload > 0 ? (double)pit_reload : 65536.0;
    const double pit_half = pit_max / 2.0;

    // --- Render samples ---

    for (ma_uint32 i = 0; i < frame_count; ++i) {

        // Determine raw output level for this sample.
        bool raw_level = false;

        if (output_enabled) {
            if (gate && pit_reload > 0) {
                switch (pit_mode) {
                    case 0: // One-shot (realsound PWM)
                        // OUT low while counting, high after terminal count.
                        raw_level = (impl->phase >= pit_max);
                        break;

                    case 1: // Retriggerable one-shot
                        raw_level = (impl->phase >= pit_max);
                        break;

                    case 2: // Rate generator
                        // OUT high for (N-1) ticks, low for 1 tick.
                        raw_level = (impl->phase < pit_max - 1.0);
                        break;

                    case 3: // Square wave
                        raw_level = impl->mode3_output;
                        break;

                    case 4: // Software triggered strobe
                        // OUT high, pulses low at terminal count.
                        raw_level = (impl->phase < pit_max - 1.0);
                        break;

                    default:
                        break;
                }
            } else if (!gate) {
                // Gate low: mode 3 forces OUT high, others freeze.
                if (pit_mode == 3) raw_level = true;
            }
        }

        // Advance phase accumulator.
        if (gate || pit_mode == 1 || pit_mode == 5) {
            impl->phase += impl->phase_inc;

            switch (pit_mode) {
                case 3: // Toggle at half-period boundary.
                    while (impl->phase >= pit_half) {
                        impl->phase -= pit_half;
                        impl->mode3_output = !impl->mode3_output;
                    }
                    break;

                case 2: // Wrap at full period.
                    while (impl->phase >= pit_max)
                        impl->phase -= pit_max;
                    break;

                case 0: case 1: // Clamp: stays high after terminal count.
                    break;

                case 4: // Wrap at full period.
                    while (impl->phase >= pit_max)
                        impl->phase -= pit_max;
                    break;

                default:
                    if (impl->phase >= pit_max)
                        impl->phase -= pit_max;
                    break;
            }
        }

        // Volume ramp (speaker cone inertia).
        double target = raw_level ? SPKR_VOLUME : 0.0;
        double diff   = target - impl->volcur;
        double step   = impl->ramp_per_sample;
        if (diff > step)       impl->volcur += step;
        else if (diff < -step) impl->volcur -= step;
        else                   impl->volcur  = target;

        // One-pole lowpass (tame aliasing).
        impl->lp_state += LP_ALPHA * (impl->volcur - impl->lp_state);

        out[i] = static_cast<float>(impl->lp_state);
    }
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

PCSpeaker::~PCSpeaker() { shutdown(); }

bool PCSpeaker::init(uint32_t sample_rate) {
    if (impl_) return false;

    impl_ = new Impl;
    impl_->sample_rate     = sample_rate;
    impl_->pit             = pit_;
    impl_->params          = &params_;
    impl_->phase_inc       = PIT_TICK_RATE / static_cast<double>(sample_rate);
    impl_->ramp_per_sample = RAMP_PER_SAMPLE_48K * (48000.0 / sample_rate);

    ma_device_config cfg   = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format    = ma_format_f32;
    cfg.playback.channels  = 1;
    cfg.sampleRate         = sample_rate;
    cfg.dataCallback       = audio_callback;
    cfg.pUserData          = impl_;
    cfg.periodSizeInFrames = 256;

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

    std::printf("[PCSpeaker] audio initialized (%u Hz, mono, period=%u)\n",
                sample_rate, 256u);
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
