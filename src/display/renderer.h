#pragma once
// Renderer -- DX11 window with ImGui overlay and all debug windows.
//
// Runs on its own thread.  Reads the framebuffer directly (Option A:
// no synchronization, no double buffer).  The emulator writes to the
// buffer on the scheduler thread; the display reads it on the render
// thread.  Tearing is invisible at 8088 write rates.
//
// Usage:
//   Renderer renderer;
//   renderer.start(mda.framebuffer());  // launches render thread
//   // ... emulator runs ...
//   renderer.stop();

#include <cstdint>
#include <thread>
#include <atomic>
#include <latch>
#include <unordered_map>
#include <string>

namespace bench {

class Scheduler;
class IC_8088;
class IC_8237A;
class IC_8253;
class IC_8259A;
class IC_8284A;
class ISA_MDA;
class ISA_CGA;
class ISA_FloppyController;
class MemoryView;
class TestKeyboard;

// Snapshot of ISA slot and RAM info for System window display.
struct SystemInfo {
    struct SlotInfo {
        std::string ref;    // "J1".."J5"
        std::string card;   // empty if unoccupied
    };
    SlotInfo slots[5] = {};
    int expansion_kb = 0;
};

// Pool indices for bus analyzer display.
struct BusProbe {
    int ad = 0;    // AD0-AD7 block base (8)
    int d = 0;     // D0-D7 block base (8)
    int xd = 0;    // XD0-XD7 block base (8)
    int la = 0;    // LA0-LA19 block base (20)
    int md = 0;    // MD0-MD7 block base (8)
    // Individual signals (pool index)
    int ale = 0, den = 0, dtr = 0;
    int memr = 0, memw = 0, ior = 0, iow = 0;
    int ready = 0, clk = 0, reset = 0;
    int hrq = 0, holda = 0, aen_brd = 0, aen_bar = 0;
    int s0 = 0, s1 = 0, s2 = 0;
    int dack0 = 0, dack1 = 0, dack2 = 0, dack3 = 0;
    int drq0 = 0, drq1 = 0, drq2 = 0, drq3 = 0;
    int intr = 0, nmi = 0;
};

class Renderer {
public:
    // Start the display thread.  vram must point to a 4096-byte buffer
    // (80x25 char+attr pairs) that remains valid until stop().
    // clk_cycles points to the 8284A's monotonic cycle counter (read-only).
    // scheduler/cpu/mem are optional; if provided, enable the debugger overlay.
    // Blocks until the window is up and rendering.
    void start(const uint8_t* vram, const uint64_t* clk_cycles = nullptr,
               Scheduler* scheduler = nullptr, IC_8088* cpu = nullptr,
               const MemoryView* mem = nullptr, IC_8237A* dma = nullptr,
               const ISA_MDA* mda_card = nullptr,
               const BusProbe* bus = nullptr,
               TestKeyboard* kbd = nullptr,
               ISA_FloppyController* fdc = nullptr,
               const ISA_CGA* cga = nullptr,
               IC_8284A* clk_gen = nullptr,
               const SystemInfo& sys_info = {},
               const IC_8259A* pic = nullptr,
               const IC_8253* pit = nullptr);

    // Bind BRD net names to live signals for board view.
    // Call after start() returns (board_view is initialized by then).
    void bind_board_signals(const std::unordered_map<std::string, int>& brd_map);

    // Stop the display thread and close the window.
    void stop();

    bool running() const { return running_.load(); }

    // Set initial disk image path for drive A (before start()).
    void set_disk_a_path(const std::string& path);

    // Poll for pending load request (set by Load State button).
    // Returns empty string if no request. Clears the request.
    std::string take_pending_load();

private:
    std::jthread thread_;
    std::atomic<bool> running_{false};
    std::latch ready_{1};
    const uint8_t* vram_ = nullptr;
    const uint64_t* clk_cycles_ = nullptr;
    Scheduler* scheduler_ = nullptr;
    IC_8088* cpu_ = nullptr;
    const MemoryView* mem_ = nullptr;
    IC_8237A* dma_ = nullptr;
    const IC_8259A* pic_ = nullptr;
    const IC_8253* pit_ = nullptr;
    const ISA_MDA* mda_card_ = nullptr;
    const BusProbe* bus_probe_ = nullptr;
    const ISA_CGA* cga_ = nullptr;
    TestKeyboard* kbd_ = nullptr;
    ISA_FloppyController* fdc_ = nullptr;
    IC_8284A* clk_gen_ = nullptr;
    SystemInfo sys_info_;
    std::string disk_a_path_;
    bool dbg_visible_ = true;

public:
    // Load request (render thread -> main thread)
    std::atomic<bool> load_requested_{false};
    std::string load_path_;
private:

    // Pending board signal binding (set from main thread, consumed by render thread).
    std::unordered_map<std::string, int> pending_brd_map_;
    std::atomic<bool> brd_map_ready_{false};

    void render_loop(std::stop_token stop);
};

} // namespace bench
