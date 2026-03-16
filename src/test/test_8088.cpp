// 8088 CPU test bench -- data-driven instruction tests.
//
// The CPU is real -- full pin-level, CLK-driven, threaded.
// The 8284A clock generator is real -- drives OSC/CLK/PCLK/RESET/READY.
// The 8288 bus controller is real -- decodes S0-S2 into control signals.
// The 74S245 transceivers are real -- U8 (AD<->D), U12 (D<->MD for DRAM).
// The 74S373 address latches are real -- ALE-triggered address capture (U7/U9/U10).
// The 74S138 I/O decoder is real -- U66 decodes XA5-7 into PIC/PIT/PPI/DMA chip selects.
// The 74S20 NAND gate is real -- U64 generates ~ROM_ADDR_SEL from A16-A19.
// The 74S138 ROM decoder is real -- U46 decodes A13-A15 into ROM chip selects.
// The IC_ROM_8K is real -- U33 serves FE000-FFFFF from the BIOS binary.
// The 8259A PIC is real -- signal-level interrupt handling.
// BusGlue is a reactive Component: memory + generic I/O, subscribes to CLK.
//
// Each test is a flat binary assembled by NASM, loaded at 0100:0100
// (physical 0x01100) in DRAM. DS=SS=0 after reset. Results checked at 0x0500+.

#include "test_board_wiring.h"
#include "core/signal.h"
#include "core/callback_component.h"
#include "core/fiber_component.h"
#include "core/scheduler.h"
#include "board/socket.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

using namespace bench;

// Throwaway: no-op fiber for measuring fiber dispatch overhead.
class NoopFiber : public FiberComponent {
public:
    NoopFiber() : FiberComponent("noop") {}
protected:
    void run() override {
        for (;;) yield();
    }
    void on_power_on() override {}
    void on_power_off() override {}
    void on_signal_change(Fiber) override {}
};

// =========================================================================
// BusGlue: reactive Component -- address decode + memory.
// Subscribes to CLK, detects edges, drives data/control for each T-state.
// Handles memory and generic I/O. PIC chip-select comes from U66 (74S138).
// =========================================================================
class BusGlue : public CallbackComponent {
public:
    BusGlue() : CallbackComponent("BusGlue") { set_description("Bus Glue"); }

    Pin xa[20];                  // XA0-XA19 -- latched address from 74S373s
    Pin d[8];                    // D0-D7 -- system data bus
    Pin pin_s0, pin_s1, pin_s2;
    std::unique_ptr<uint8_t[]> io  = std::make_unique<uint8_t[]>(1 << 16);

    Signal* pic_ir[8] = {};       // IR0-IR7 (for test trigger port 0xF0)

    void init(Signal* xa_sigs[], Signal* d_sigs[], Signal& s0, Signal& s1, Signal& s2, Signal& clk) {
        for (int i = 0; i < 20; ++i) xa[i] = xa_sigs[i]->pin();
        for (int i = 0; i < 8; ++i)  d[i]  = d_sigs[i]->pin();
        pin_s0 = s0.pin();
        pin_s1 = s1.pin();
        pin_s2 = s2.pin();
        clk.connect(this);

        // Pin directions for wiring visualization.
        for (int i = 0; i < 20; ++i) declare_input(xa[i]);
        for (int i = 0; i < 8; ++i) { declare_input(d[i]); declare_output(d[i]); }
        declare_async_input(pin_s0); declare_async_input(pin_s1); declare_async_input(pin_s2);
        declare_input(clk.pin());

        // D pins are bidirectional: read cycles drive D (output), write cycles read D (input).
        declare_bidir_block({d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]},
                            [this]() { return is_read_cycle() ? BidirDir::Output : BidirDir::Input; });
    }

    // T-state machine
    enum class TState { IDLE, T1, T2, T3, T4 };
    TState t_state = TState::IDLE;
    uint8_t cycle_type = 7;
    uint32_t cycle_addr = 0;
    int bus_cycle_count = 0;

    void reset() {
        t_state = TState::IDLE;
        cycle_type = 7;
        cycle_addr = 0;
        bus_cycle_count = 0;
    }

protected:
    void on_signal_change(Fiber caller) override {
        on_clk_rising();
        on_clk_falling();
    }

private:
    uint8_t decode_status() {
        uint8_t v2 = (pin_s2.level() == Level::High) ? 1 : 0;
        uint8_t v1 = (pin_s1.level() == Level::High) ? 1 : 0;
        uint8_t v0 = (pin_s0.level() == Level::High) ? 1 : 0;
        return (v2 << 2) | (v1 << 1) | v0;
    }

    uint32_t read_address() {
        uint32_t addr = 0;
        for (int i = 0; i < 20; ++i)
            if (xa[i].level() == Level::High)
                addr |= (1u << i);
        return addr;
    }

    uint8_t read_d() {
        uint8_t val = 0;
        for (int i = 0; i < 8; ++i)
            if (d[i].level() == Level::High)
                val |= (1u << i);
        return val;
    }

    void drive_d(uint8_t val) {
        for (int i = 0; i < 8; ++i)
            d[i].drive((val >> i) & 1 ? Level::High : Level::Low);
    }

    void release_d() {
        for (int i = 0; i < 8; ++i)
            d[i].release();
    }

    bool is_read_cycle()  { return cycle_type == 0 || cycle_type == 1 || cycle_type == 4 || cycle_type == 5; }
    bool is_write_cycle() { return cycle_type == 2 || cycle_type == 6; }
    bool is_io_cycle()    { return cycle_type == 1 || cycle_type == 2; }
    bool is_inta_cycle()  { return cycle_type == 0; }
    // DMA (0x00-0x1F) and PIC (0x20-0x3F) are decoded by U66 (74S138).
    // Real ICs handle these ports; BusGlue must not drive the data bus.
    bool is_hw_decoded(uint32_t addr) { return (addr & 0xFFFF) <= 0x3F; }


    uint8_t io_read(uint16_t port) { return io[port]; }

    void io_write(uint16_t port, uint8_t val) {
        if (port == 0xF0) {
            // Test trigger port: drive IR lines High.
            // The 8284A's wait_quiescent after this CLK edge ensures the PIC
            // processes the rising edge before the next CLK tick.
            for (int i = 0; i < 8; i++) {
                if ((val & (1 << i)) && pic_ir[i])
                    pic_ir[i]->drive(Level::High);
            }
        } else if (port == 0xF1) {
            // Test clear port: drive IR lines Low.
            for (int i = 0; i < 8; i++) {
                if ((val & (1 << i)) && pic_ir[i])
                    pic_ir[i]->drive(Level::Low);
            }
        } else {
            io[port] = val;
        }
    }

    void on_clk_rising() {
        uint8_t status = decode_status();

        switch (t_state) {
        case TState::IDLE:
            if (status != 7) {
                t_state = TState::T1;
                cycle_type = status;
                cycle_addr = read_address();
            }
            break;

        case TState::T1:
            t_state = TState::T2;
            break;

        case TState::T2:
            t_state = TState::T3;
            if (is_read_cycle()) {
                // DRAM reads: U12 (74S245) bridges MD->D automatically.
                // INTA: PIC drives D via ~CS from U66.
                // HW-decoded IO: handled by real ICs.
                if (!is_inta_cycle()
                    && is_io_cycle() && !is_hw_decoded(cycle_addr)) {
                    drive_d(io_read(cycle_addr & 0xFFFF));
                }
            } else if (is_write_cycle()) {
                // DRAM writes: U12 (74S245) bridges D->MD automatically.
                if (is_io_cycle() && !is_hw_decoded(cycle_addr)) {
                    io_write(cycle_addr & 0xFFFF, read_d());
                }
            }
            break;

        case TState::T3:
            t_state = TState::T4;
            break;

        case TState::T4:
            if (status != 7) {
                t_state = TState::T1;
                cycle_type = status;
                bus_cycle_count++;
                cycle_addr = read_address();
            } else {
                if (is_read_cycle()) {
                    release_d();
                }
                bus_cycle_count++;
                t_state = TState::IDLE;
            }
            break;
        }
    }

    void on_clk_falling() {
        // DRAM write data transfer (D->MD) handled by U12 (74S245).
    }
};

// =========================================================================
// Test definition: binary file + expected memory values.
// =========================================================================
// 74S158 inverts both row and column address bits. Physical address and
// DRAM internal array offset differ because preload/readback bypass the
// pin interface. This maps physical -> internal for those paths.
static uint32_t dram_xlat(uint32_t phys) {
    uint8_t row = static_cast<uint8_t>(~(phys & 0xFF));
    uint8_t col = static_cast<uint8_t>(~((phys >> 8) & 0xFF));
    uint32_t bank = (phys >> 16) & 3;
    return (bank << 16) | (static_cast<uint32_t>(row) << 8) | col;
}

struct Expect { uint32_t addr; uint16_t value; std::string label; };
struct Dump   { uint32_t addr; std::string label; };

struct TestCase {
    std::string name;
    std::string bin_file;
    std::vector<Expect> expects;
    std::vector<Dump>   dumps;
};

// Parse @name and @expect tags from an assembly source file.
// Returns a TestCase with name from @name, bin_file from short_name,
// and expects from @expect lines.
static TestCase parse_test_asm(const std::string& asm_dir, const std::string& short_name) {
    TestCase tc;
    tc.bin_file = "test_" + short_name + ".bin";
    tc.name = short_name;  // fallback if no @name tag

    std::string asm_path = asm_dir + "/test_" + short_name + ".asm";
    std::ifstream f(asm_path);
    if (!f) {
        spdlog::error("cannot open asm file: {}", asm_path);
        return tc;
    }

    std::string line;
    while (std::getline(f, line)) {
        // Strip trailing \r (Windows line endings).
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // ; @name Display Name
        if (line.rfind("; @name ", 0) == 0) {
            tc.name = line.substr(8);
            continue;
        }
        // ; @expect ADDR VALUE label text
        if (line.rfind("; @expect ", 0) == 0) {
            // Format: ADDR(4hex) SPACE VALUE(4hex) SPACE label...
            const char* p = line.c_str() + 10;
            uint32_t addr = 0;
            uint16_t value = 0;
            int n = 0;
            if (sscanf(p, "%4x %4hx%n", &addr, &value, &n) >= 2) {
                const char* label = p + n;
                while (*label == ' ') ++label;
                tc.expects.push_back({addr, value, label});
            }
        }
        // ; @dump ADDR label text
        if (line.rfind("; @dump ", 0) == 0) {
            const char* p = line.c_str() + 7;
            uint32_t addr = 0;
            int n = 0;
            if (sscanf(p, "%4x%n", &addr, &n) >= 1) {
                const char* label = p + n;
                while (*label == ' ') ++label;
                tc.dumps.push_back({addr, label});
            }
        }
    }
    return tc;
}

static bool load_bin(const std::string& path, uint8_t* mem, uint32_t load_addr, uint32_t mem_size) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        spdlog::error("  cannot open {}", path);
        return false;
    }
    f.seekg(0, std::ios::end);
    auto size = f.tellg();
    f.seekg(0);
    if (load_addr + size > mem_size) {
        spdlog::error("  binary too large: {} bytes at 0x{:05X}", (int)size, load_addr);
        return false;
    }
    // Scatter-write through 74S158 address inversion so that pin-level
    // DRAM accesses (which see inverted row/col) find the correct data.
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(buf.data()), size);
    for (size_t i = 0; i < buf.size(); ++i)
        mem[dram_xlat(load_addr + static_cast<uint32_t>(i))] = buf[i];
    spdlog::debug("  loaded {} bytes at 0x{:05X} (translated)", (int)size, load_addr);
    return true;
}

int main() {
    spdlog::set_level(spdlog::level::trace);
    spdlog::info("=== 8088 Test Bench ===");
    spdlog::info("ASM_TEST_DIR: {}", ASM_TEST_DIR);

    // Test list -- names correspond to test_<name>.asm / test_<name>.bin.
    // Expected results are parsed from @name / @expect tags in the asm files.
    std::vector<std::string> test_names = {
        //"mov", "alu", "call_ret", "jumps", "int", "string",
        //"mul", "bcd", "farcall", "io", "div", "dos", "irq", "rom",
        "pit",
    };

    std::vector<TestCase> tests;
    for (auto& name : test_names)
        tests.push_back(parse_test_asm(ASM_SRC_DIR, name));

    // --- Wire the test board (signals, sockets, ICs) ---
    std::string bios_path = std::string(ASSETS_DIR) + "/BIOS_IBM5150_27OCT82_1501476_U33.BIN";
    TestBoard board;
    board.wire(bios_path);

    // Convenience aliases for test code below.
    auto* clk_gen = board.clk_gen;
    auto* cpu = board.cpu;
    auto& dram = board.dram;
    auto& all_traces = board.all_traces;

    // BusGlue: reactive address decode + memory
    Signal* xa_ptrs[20];
    Signal* d_ptrs[8];
    for (int i = 0; i < 20; ++i) xa_ptrs[i] = &board.xa[i];
    for (int i = 0; i < 8; ++i)  d_ptrs[i] = board.d_arr[i];

    BusGlue bus;
    bus.init(xa_ptrs, d_ptrs, board.s0, board.s1, board.s2, board.clk);

    for (int i = 0; i < 8; ++i) {
        bus.pic_ir[i] = board.irq_arr[i];
        bus.declare_output(board.irq_arr[i]->pin());
    }

    // Scheduler: commits signals, evals inline ICs, runs fiber components.
    // The 8284A calls scheduler.evaluate(self) at each CLK edge from its spin loop.
    // All fiber components run cooperatively on the 8284A's thread.
    Scheduler scheduler;
    Signal::set_scheduler(&scheduler);
    board.register_all(scheduler);
    scheduler.register_callback(&bus);
    // Resolve callback dependency graph (DRAM outputs -> BusGlue inputs).
    // Must be called after all register_*() calls so dump_dot sees everything.
    scheduler.resolve();

    // --- Run tests (power cycle between each) ---
    int passed = 0, failed = 0;

    for (auto& tc : tests) {
        spdlog::info("--- {} ---", tc.name);

        // Reset I/O space, DRAM, and BusGlue state
        std::memset(bus.io.get(), 0xFF, 1 << 16);
        std::memset(dram.data(), 0xF4, IC_DRAM_256K::size());
        bus.reset();

        // Load binary into DRAM at 0100:0100 (physical 0x01100)
        std::string path = std::string(ASM_TEST_DIR) + "/" + tc.bin_file;
        if (!load_bin(path, dram.data(), 0x1100, IC_DRAM_256K::size())) { ++failed; continue; }

        // Verify load (each physical addr translates independently)
        {
            auto d = dram.data();
            spdlog::trace("  dram phys 01100..01107 = {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
                d[dram_xlat(0x1100)], d[dram_xlat(0x1101)], d[dram_xlat(0x1102)], d[dram_xlat(0x1103)],
                d[dram_xlat(0x1104)], d[dram_xlat(0x1105)], d[dram_xlat(0x1106)], d[dram_xlat(0x1107)]);
        }

        // Power on: 8284A thread starts, PSU powers all components, drives VCC.
        cpu->clear_halt();
#ifdef BENCH_PIN_VALIDATION
        SignalPool::enable_validation();
#endif
        clk_gen->power_on();
        clk_gen->psu_power_on();

        // Wait for CPU to halt, with 10s safety timeout.
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (!cpu->halted() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (!cpu->halted())
                spdlog::warn("  timeout -- CPU did not halt within 1s");
        }

        // Power off: PSU drops VCC, 8284A stops clock and powers off all components.
        clk_gen->psu_power_off();
        clk_gen->power_off();

        // Power loss: every trace on the board discharges.
        for (auto* sig : all_traces)
            sig->reset();

        // Check results -- read from DRAM through 74S158 address translation.
        bool pass = true;
        const uint8_t* ram = dram.data();
        for (auto& e : tc.expects) {
            uint16_t actual = ram[dram_xlat(e.addr)] | (ram[dram_xlat(e.addr + 1)] << 8);
            if (actual != e.value) {
                spdlog::error("  FAIL {}: [0x{:04X}] = 0x{:04X} (expected 0x{:04X})",
                    e.label, e.addr, actual, e.value);
                pass = false;
            } else {
                spdlog::info("  ok   {}: [0x{:04X}] = 0x{:04X}",
                    e.label, e.addr, actual);
            }
        }
        for (auto& d : tc.dumps) {
            uint16_t actual = ram[dram_xlat(d.addr)] | (ram[dram_xlat(d.addr + 1)] << 8);
            spdlog::info("  dump {}: [0x{:04X}] = 0x{:04X} ({})",
                d.label, d.addr, actual, actual);
        }
        if (pass) ++passed; else ++failed;
    }

    spdlog::info("=== Results: {} passed, {} failed ===", passed, failed);

//#define RUN_BENCHMARK

#if defined(RUN_BENCHMARK)
    // --- Benchmark: 64-bit increment loop, timed by NMI ---
    constexpr int BENCH_SECONDS = 5;
    spdlog::info("--- Benchmark: 64-bit increment ({} seconds) ---", BENCH_SECONDS);
    {
        board.dma_enabled = false;
        std::memset(bus.io.get(), 0xFF, 1 << 16);
        std::memset(dram.data(), 0xF4, IC_DRAM_256K::size());
        bus.reset();

        std::string path = std::string(ASM_TEST_DIR) + "/test_bench64.bin";
        if (!load_bin(path, dram.data(), 0x1100, IC_DRAM_256K::size())) {
            spdlog::error("  benchmark skipped -- cannot load binary");
        }
        else {
            cpu->clear_halt();
#ifdef BENCH_PIN_VALIDATION
            SignalPool::enable_validation();
#endif
            clk_gen->power_on();
            clk_gen->psu_power_on();

            // Let it run for BENCH_SECONDS, then fire NMI.
            auto start = std::chrono::steady_clock::now();
            auto deadline = start + std::chrono::seconds(BENCH_SECONDS);
            std::this_thread::sleep_for(std::chrono::seconds(BENCH_SECONDS));
            clk_gen->psu_nmi_raise();

            // Wait for CPU to halt (NMI handler does HLT).
            auto halt_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (!cpu->halted() && std::chrono::steady_clock::now() < halt_deadline)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            auto end = std::chrono::steady_clock::now();

            double elapsed = std::chrono::duration<double>(end - start).count();

            if (!cpu->halted())
                spdlog::warn("  benchmark timeout -- CPU did not halt");

            // Power off.
            clk_gen->psu_nmi_lower();
            clk_gen->psu_power_off();
            clk_gen->power_off();

            // Read 64-bit counter from DRAM (through 74S158 translation).
            uint64_t count = 0;
            for (int i = 0; i < 8; ++i)
                count |= (uint64_t)dram.data()[dram_xlat(0x0500 + i)] << (i * 8);

            double rate = (elapsed > 0) ? (double)count / elapsed : 0;
            uint64_t cycles = clk_gen->clk_cycles();
            double mhz = (elapsed > 0) ? (double)cycles / elapsed / 1e6 : 0;
            spdlog::info("  count: {} increments in {:.3f}s ({:.0f} inc/s)",
                count, elapsed, rate);
            spdlog::info("  CLK: {} cycles ({:.3f} MHz, target 4.77 MHz)",
                cycles, mhz);

            for (auto* sig : all_traces)
                sig->reset();
        }
        board.dma_enabled = true;
    }
#endif

    #define DUMP_PERMUTATIONS

#if defined(DUMP_PERMUTATIONS)
    spdlog::info("--- Dumping encountered DAG permutations as SVG ---");
    scheduler.dump_permutation_svgs();
#endif


    return failed > 0 ? 1 : 0;
}
