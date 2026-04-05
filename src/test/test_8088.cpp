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
// ISA Test Card: expansion card in J1 -- handles I/O ports > 0x7F, test IRQ triggers.
//
// Each test is a flat binary assembled by NASM, loaded at 0100:0100
// (physical 0x01100) in DRAM. DS=SS=0 after reset. Results checked at 0x0500+.

#include "board/board_wiring.h"
#include "isa/isa_bus.h"
#include "isa/isa_testcard.h"
#include "isa/isa_fdc.h"
#include "isa/isa_mda.h"
#include "isa/isa_ram.h"
#include "test_keyboard.h"
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
#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif
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
    void on_cycle(Fiber) override {}
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
    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== 8088 Test Bench ===");
    spdlog::info("ASM_TEST_DIR: {}", ASM_TEST_DIR);

    // Test list -- names correspond to test_<name>.asm / test_<name>.bin.
    // Expected results are parsed from @name / @expect tags in the asm files.
    std::vector<std::string> test_names = {
        "mov",
        "post_ram",
        "post_sw1_readback",
        "keyboard",
        "mov",
        "dma",
        "dma_isa_ram",
        "dma_m2m",
        //"dma_refresh",
        "mov",
        "rom",
        "io",
        "alu",
        "call_ret",
        "jumps",
        "int",
        "string",
        "mul",
        "bcd",
        "farcall",
        "div",
        "dos",
        "irq",
        "pit",
        "mmio",
        "floppy",
        "io_floppy",
        "speaker",
        "memw_no_io",
        "post_cpu",
        "post_cass",
        "post_rom",
        "post_basic",
        "post_pic",
        "post_video",
        "post_dma",
        "post_fdc",
        "fdc_write",
        "post_dipsw",
        "post_pkey",
        "post_fdc_detect",
        "post_pki",
    };

    std::vector<TestCase> tests;
    for (auto& name : test_names)
        tests.push_back(parse_test_asm(ASM_SRC_DIR, name));

    // --- Wire the test board (signals, sockets, ICs) ---
    std::string bios_path = std::string(ASSETS_DIR) + "/BIOS_IBM5150_27OCT82_1501476_U33.BIN";
    std::string basic_u29 = std::string(ASSETS_DIR) + "/IBM 5150 - Cassette BASIC version C1.10 - U29 - 5000019.bin";
    std::string basic_u30 = std::string(ASSETS_DIR) + "/IBM 5150 - Cassette BASIC version C1.10 - U30 - 5000021.bin";
    std::string basic_u31 = std::string(ASSETS_DIR) + "/IBM 5150 - Cassette BASIC version C1.10 - U31 - 5000022.bin";
    std::string basic_u32 = std::string(ASSETS_DIR) + "/IBM 5150 - Cassette BASIC version C1.10 - U32 - 5000023.bin";
    Board board;
    board.wire(bios_path, basic_u29, basic_u30, basic_u31, basic_u32);

    // Convenience aliases for test code below.
    auto* clk_gen = board.clk_gen;
    auto* cpu = board.cpu;
    auto& dram = board.dram;
    auto& all_traces = board.all_traces;

    // ISA bus: single component for all expansion cards.
    ISA_Bus isa_bus;
    isa_bus.install(board.isa_slots[0]);

    // ISA Test Card: J1, I/O ports > 0x7F, DMA ch1+3, IRQ2-5,7.
    ISA_TestCard testcard;
    isa_bus.insert_card(0, &testcard, 0x0A, 0xBC);

    // Floppy Disk Controller: plugs into J2, uses DMA channel 2, IRQ 6.
    std::string dos_disk = std::string(ASSETS_DIR) + "/IBM DOS 3.30 360K Disks - Disk 01.img";
    std::vector<uint8_t> floppy_img;
    {
        std::ifstream f(dos_disk, std::ios::binary | std::ios::ate);
        if (f) {
            auto sz = f.tellg();
            floppy_img.resize(static_cast<size_t>(sz));
            f.seekg(0);
            f.read(reinterpret_cast<char*>(floppy_img.data()), sz);
            spdlog::info("[FDC] loaded {} bytes from {}", floppy_img.size(), dos_disk);
        } else {
            spdlog::warn("[FDC] disk image not found: {}", dos_disk);
        }
    }
    std::vector<uint8_t> floppy_img_backup = floppy_img;  // keep copy for reload
    ISA_FloppyController fdc(std::move(floppy_img), 9, 2);
    isa_bus.insert_card(1, &fdc, 0x04, 0x40);  // DMA ch2, IRQ6

    // MDA card: J3, framebuffer at 0xB0000.
    ISA_MDA mda;
    isa_bus.insert_card(2, &mda);

    // ISA RAM expansion: J4, 384KB at 0x40000-0x9FFFF (256KB planar + 384KB = 640KB).
    ISA_RAM isa_ram(0x40000, 384 * 1024);
    isa_bus.insert_card(3, &isa_ram);

    // Configure DIP switches from installed hardware.
    board.add_floppy_drives(2);
    board.set_video(Board::MDA);
    board.add_expansion_kb(384);
    board.compute_switches();

    // Scheduler: commits signals, evals inline ICs, runs fiber components.
    // The 8284A calls scheduler.evaluate(self) at each CLK edge from its spin loop.
    // All fiber components run cooperatively on the 8284A's thread.
    Scheduler scheduler;
    Signal::set_scheduler(&scheduler);
    board.register_all(scheduler);
    scheduler.register_callback(&isa_bus);

    // Keyboard: bypasses U24 serial shift register, drives PA0-PA7 + IRQ1 directly.
    // Armed by test program writing to testcard port 0xFC.
    testcard.set_kbd_ready_signal(&board.kbd_ready);
    testcard.set_kbd_ack_signal(&board.kbd_ack);
    TestKeyboard keyboard;
    testcard.set_keyboard(&keyboard);
    {
        Signal* pa_ptrs[8];
        for (int i = 0; i < 8; ++i) pa_ptrs[i] = &board.ppi_pa[i];
        keyboard.connect(pa_ptrs, board.irq1, board.ppi_pb[6],
                         board.ppi_pb[7], board.kbd_ready, board.kbd_ack);
    }
    scheduler.register_callback(&keyboard);

    // Resolve callback dependency graph.
    // Must be called after all register_*() calls so dump_dot sees everything.
    scheduler.resolve();

    // --- Run tests (power cycle between each) ---
    int passed = 0, failed = 0;

    for (auto& tc : tests) {
        spdlog::info("--- {} ---", tc.name);

        // Reset I/O space, DRAM, and expansion RAM (IC state resets in on_power_on)
        std::memset(testcard.io_data(), 0xFF, 1 << 16);
        std::memset(dram.data(), 0x00, IC_DRAM_256K::size());
        std::memset(const_cast<uint8_t*>(isa_ram.data()), 0x00, isa_ram.size());

        // Preload DMA buffer for the DMA test.
        {
            static const char lorem[] = "Lorem ipsum dolor sit amet, ";
            std::memcpy(testcard.dma_buf(), lorem, sizeof(lorem) - 1);
        }

        // FDC write tests: swap in a blank 360K image to avoid corrupting DOS disk.
        bool fdc_write_test = (tc.bin_file.find("fdc_write") != std::string::npos);
        if (fdc_write_test) {
            std::vector<uint8_t> blank(360 * 1024, 0x00);
            fdc.load_image(std::move(blank), 9, 2);
        } else {
            // Reload the DOS image (previous write test may have modified it).
            fdc.load_image(floppy_img_backup, 9, 2);
        }

        // Load binary into DRAM at 0100:0100 (physical 0x01100)
        std::string path = std::string(ASM_TEST_DIR) + "/" + tc.bin_file;
        if (!load_bin(path, dram.data(), 0x1100, IC_DRAM_256K::size())) { ++failed; continue; }

        // Power on: 8284A thread starts, PSU powers all components, drives VCC.
        cpu->set_reset_vector(0x0100, 0x0100);
        cpu->clear_halt();
        cpu->clear_breakpoint();
#ifdef BENCH_PIN_VALIDATION
        SignalPool::enable_validation();
#endif

        clk_gen->power_on();
        clk_gen->psu_power_on();

        // Wait for CPU to halt, with safety timeout (disabled under debugger).
        {
#ifdef _WIN32
            bool debugger = ::IsDebuggerPresent();
#else
            bool debugger = false;
#endif
            constexpr uint64_t secondTimeout = 60;
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(secondTimeout);
            while (!cpu->breakpoint() && !cpu->halted() && (debugger || std::chrono::steady_clock::now() < deadline))
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (!cpu->breakpoint() && !cpu->halted())
                spdlog::warn("  timeout -- CPU did not halt/breakpoint within {}s", secondTimeout);
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

#define RUN_BENCHMARK

#if defined(RUN_BENCHMARK)
    // --- Benchmark: 64-bit increment loop, timed by NMI ---
    constexpr int BENCH_SECONDS = 5;
    spdlog::info("--- Benchmark: 64-bit increment ({} seconds) ---", BENCH_SECONDS);
    {
        board.dma_enabled = true;
        std::memset(testcard.io_data(), 0xFF, 1 << 16);
        std::memset(dram.data(), 0x00, IC_DRAM_256K::size());

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

    //#define DUMP_PERMUTATIONS

#if defined(DUMP_PERMUTATIONS)
    spdlog::info("--- Dumping encountered DAG permutations as SVG ---");
    scheduler.dump_permutation_svgs();
#endif


    return failed > 0 ? 1 : 0;
}
