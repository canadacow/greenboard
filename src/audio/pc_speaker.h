#pragma once
// PC speaker audio -- real-time waveform synthesis via miniaudio.
//
// The simulation thread (8255A / 8253) writes speaker parameters into
// a plain shared struct (SpeakerParams).  The audio callback reads them
// without synchronization -- same lockless pattern as CGA scanline_regs.
//
// The audio callback runs its own PIT phase accumulator to synthesize
// the correct waveform shape (mode 3 square wave, mode 0 PWM, etc.)
// based on the current PIT channel 2 mode and reload value.
//
// Usage:
//   PCSpeaker spk;
//   spk.set_pit(&pit_8253);
//   spk.init();
//   // simulation writes spk.params() from 8255A on PB0/PB1 changes
//   spk.shutdown();

#include <cstdint>

namespace bench {

class IC_8253;

// Shared state between simulation thread and audio thread.
// Written by IC_8255A (sim thread), read by audio callback.
// No synchronization -- CGA display pattern.
struct SpeakerParams {
    bool pit_gate = false;            // PB0: gates PIT channel 2
    bool pit_output_enabled = false;  // PB1: speaker output enable
};

class PCSpeaker {
public:
    PCSpeaker() = default;
    ~PCSpeaker();

    PCSpeaker(const PCSpeaker&) = delete;
    PCSpeaker& operator=(const PCSpeaker&) = delete;

    // Initialize audio device and start playback.
    bool init(uint32_t sample_rate = 48000);
    void shutdown();

    // Set PIT pointer for reading channel 2 state (mode, reload).
    // Must be called before init().
    void set_pit(const IC_8253* pit) { pit_ = pit; }

    // Shared params -- 8255A writes these from sim thread.
    SpeakerParams& params() { return params_; }
    const SpeakerParams& params() const { return params_; }

    struct Impl;
private:
    Impl* impl_ = nullptr;
    const IC_8253* pit_ = nullptr;
    SpeakerParams params_;
};

} // namespace bench
