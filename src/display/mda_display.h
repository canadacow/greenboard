#pragma once
// MDA Display — DX12 window that reads an 80x25 VRAM buffer at 60fps.
//
// Runs on its own thread.  Reads the framebuffer directly (Option A:
// no synchronization, no double buffer).  The emulator writes to the
// buffer on the scheduler thread; the display reads it on the render
// thread.  Tearing is invisible at 8088 write rates.
//
// Usage:
//   MdaDisplay display;
//   display.start(mda.framebuffer());  // launches render thread
//   // ... emulator runs ...
//   display.stop();

#include <cstdint>
#include <thread>
#include <atomic>
#include <latch>

namespace bench {

class MdaDisplay {
public:
    // Start the display thread.  vram must point to a 4096-byte buffer
    // (80x25 char+attr pairs) that remains valid until stop().
    // clk_cycles points to the 8284A's monotonic cycle counter (read-only).
    // Blocks until the window is up and rendering.
    void start(const uint8_t* vram, const uint64_t* clk_cycles = nullptr);

    // Stop the display thread and close the window.
    void stop();

    bool running() const { return running_.load(); }

private:
    std::jthread thread_;
    std::atomic<bool> running_{false};
    std::latch ready_{1};
    const uint8_t* vram_ = nullptr;
    const uint64_t* clk_cycles_ = nullptr;

    void render_loop(std::stop_token stop);
};

} // namespace bench
