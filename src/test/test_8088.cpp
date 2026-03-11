// 8088 CPU test bench -- data-driven instruction tests.
//
// The CPU is real -- full pin-level, CLK-driven, threaded.
// Everything else is combinational: BusGlue emulates the 8288 + address
// latches + data transceiver + memory as instant logic. It runs
// synchronously inside TestClock's drive loop, BEFORE the CLK edge
// reaches the CPU's mailbox. This guarantees BusGlue sees the bus state
// and drives data before the CPU reads it.
//
// Each test is a flat binary assembled by NASM, loaded at F000:0123
// (physical 0xF0123). DS=SS=0 after reset. Results checked at 0x0200+.

#include "core/signal.h"
#include "core/component.h"
#include "board/socket.h"
#include "ic/ic_8088.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

using namespace bench;

// =========================================================================
// BusGlue: combinational 8288 + latches + transceiver + 1MB flat memory.
// NOT a Component. Called synchronously by TestClock.
// =========================================================================
struct BusGlue {
    Signal** ad;           // AD0-AD7 (8 pointers)
    Signal** a_upper;      // A8-A19 (12 pointers)
    Signal* s0;
    Signal* s1;
    Signal* s2;
    uint8_t mem[1 << 20];

    // T-state machine (mirrors 8288 bus controller)
    enum class TState { IDLE, T1, T2, T3, T4 };
    TState t_state = TState::IDLE;
    uint8_t cycle_type = 7;   // status code for current cycle
    uint32_t cycle_addr = 0;

    uint8_t decode_status() {
        uint8_t v2 = (s2->level() == Level::High) ? 1 : 0;
        uint8_t v1 = (s1->level() == Level::High) ? 1 : 0;
        uint8_t v0 = (s0->level() == Level::High) ? 1 : 0;
        return (v2 << 2) | (v1 << 1) | v0;
    }

    uint32_t read_address() {
        uint32_t addr = 0;
        for (int i = 0; i < 8; ++i)
            if (ad[i] && ad[i]->level() == Level::High)
                addr |= (1u << i);
        for (int i = 0; i < 12; ++i)
            if (a_upper[i] && a_upper[i]->level() == Level::High)
                addr |= (1u << (i + 8));
        return addr;
    }

    uint8_t read_ad() {
        uint8_t val = 0;
        for (int i = 0; i < 8; ++i)
            if (ad[i] && ad[i]->level() == Level::High)
                val |= (1u << i);
        return val;
    }

    void drive_ad(uint8_t val) {
        for (int i = 0; i < 8; ++i)
            if (ad[i])
                ad[i]->drive((val >> i) & 1 ? Level::High : Level::Low);
    }

    void release_ad() {
        for (int i = 0; i < 8; ++i)
            if (ad[i]) ad[i]->release();
    }

    bool is_read_cycle() { return cycle_type == 4 || cycle_type == 5; }
    bool is_write_cycle() { return cycle_type == 2 || cycle_type == 6; }

    const char* tstate_name() {
        switch (t_state) {
        case TState::IDLE: return "IDLE";
        case TState::T1: return "T1";
        case TState::T2: return "T2";
        case TState::T3: return "T3";
        case TState::T4: return "T4";
        }
        return "?";
    }

    void on_clk_rising() {
        uint8_t status = decode_status();
        spdlog::trace("[BusGlue] CLK_RISE  state={} status={} bus_addr=0x{:05X} ad=0x{:02X}",
            tstate_name(), status, read_address(), read_ad());

        switch (t_state) {
        case TState::IDLE:
            if (status != 7) {
                t_state = TState::T1;
                cycle_type = status;
                spdlog::trace("[BusGlue]   -> T1 NEW CYCLE type={}", cycle_type);
            }
            break;

        case TState::T1:
            t_state = TState::T2;
            if (is_read_cycle()) {
                uint8_t val = mem[cycle_addr & 0xFFFFF];
                spdlog::trace("[BusGlue]   -> T2 READ drive mem[0x{:05X}]=0x{:02X}", cycle_addr, val);
                drive_ad(val);
            } else {
                spdlog::trace("[BusGlue]   -> T2");
            }
            break;

        case TState::T2:
            t_state = TState::T3;
            if (is_write_cycle()) {
                uint8_t val = read_ad();
                spdlog::trace("[BusGlue]   -> T3 WRITE mem[0x{:05X}]=0x{:02X}", cycle_addr, val);
                mem[cycle_addr & 0xFFFFF] = val;
            } else {
                spdlog::trace("[BusGlue]   -> T3");
            }
            break;

        case TState::T3:
            if (status == 7) {
                t_state = TState::T4;
                spdlog::trace("[BusGlue]   -> T4 (status passive)");
            } else {
                spdlog::trace("[BusGlue]   Tw (status={}, waiting)", status);
            }
            break;

        case TState::T4:
            if (status != 7) {
                t_state = TState::T1;
                cycle_type = status;
                spdlog::trace("[BusGlue]   -> T1 NEW CYCLE (overlapped T4) type={}", cycle_type);
            } else {
                if (is_read_cycle()) {
                    release_ad();
                }
                t_state = TState::IDLE;
                spdlog::trace("[BusGlue]   -> IDLE");
            }
            break;
        }
    }

    void on_clk_falling() {
        spdlog::trace("[BusGlue] CLK_FALL  state={} bus_addr=0x{:05X} ad=0x{:02X}",
            tstate_name(), read_address(), read_ad());
        if (t_state == TState::T1) {
            cycle_addr = read_address();
            spdlog::trace("[BusGlue]   ALE latch addr=0x{:05X}", cycle_addr);
        }
    }

    void reset() {
        t_state = TState::IDLE;
        cycle_type = 7;
        cycle_addr = 0;
    }
};

// =========================================================================
// TestClock: drives CLK AND calls BusGlue synchronously each tick.
// =========================================================================
class TestClock : public Component {
public:
    TestClock(Signal& clk, BusGlue& bus)
        : Component("TestClock"), clk_(clk), bus_(bus) {}

    void run(std::stop_token stop) override {
        int tick = 0;
        while (!stop.stop_requested()) {
            Signal::wait_quiescent(stop);
            if (stop.stop_requested()) break;

            spdlog::trace("[CLK] ---- tick {} ---- PRE-RISE", tick);
            bus_.on_clk_rising();
            spdlog::trace("[CLK] ---- tick {} ---- DRIVE HIGH", tick);
            clk_.drive(Level::High);

            Signal::wait_quiescent(stop);
            if (stop.stop_requested()) break;

            spdlog::trace("[CLK] ---- tick {} ---- PRE-FALL", tick);
            bus_.on_clk_falling();
            spdlog::trace("[CLK] ---- tick {} ---- DRIVE LOW", tick);
            clk_.drive(Level::Low);
            tick++;
        }
    }
    void on_signal_change() override {}
private:
    Signal& clk_;
    BusGlue& bus_;
};

// =========================================================================
// Test definition: binary file + expected memory values.
// =========================================================================
struct Expect { uint32_t addr; uint16_t value; const char* label; };

struct TestCase {
    const char* name;
    const char* bin_file;
    std::vector<Expect> expects;
};

static bool load_bin(const std::string& path, uint8_t* mem, uint32_t load_addr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        spdlog::error("  cannot open {}", path);
        return false;
    }
    f.seekg(0, std::ios::end);
    auto size = f.tellg();
    f.seekg(0);
    if (load_addr + size > (1 << 20)) {
        spdlog::error("  binary too large: {} bytes at 0x{:05X}", (int)size, load_addr);
        return false;
    }
    f.read(reinterpret_cast<char*>(mem + load_addr), size);
    spdlog::debug("  loaded {} bytes at 0x{:05X}", (int)size, load_addr);
    return true;
}

int main() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("=== 8088 Test Bench ===");
    spdlog::info("ASM_TEST_DIR: {}", ASM_TEST_DIR);

    // Test table
    std::vector<TestCase> tests = {
        {"MOV/XCHG", "test_mov.bin", {
            {0x0200, 0x1234, "MOV imm16"},
            {0x0202, 0x5678, "MOV reg-reg"},
            {0x0204, 0x00AB, "MOV byte"},
            {0x0206, 0xDEF0, "XCHG ax"},
            {0x0208, 0x9ABC, "XCHG bx"},
        }},
        {"ALU", "test_alu.bin", {
            {0x0200, 0x0042, "ADD"},
            {0x0202, 0x0010, "SUB"},
            {0x0204, 0xFFBE, "NEG"},
            {0x0206, 0x1234, "AND"},
            {0x0208, 0xFFFF, "OR"},
            {0x020A, 0xEDCB, "XOR"},
            {0x020C, 0xEDCA, "NOT"},
            {0x020E, 0x2468, "SHL"},
            {0x0210, 0x048D, "SHR"},
            {0x0212, 0x0001, "CMP/JE"},
            {0x0214, 0x008A, "ADC"},
            {0x0216, 0x00FE, "SBB"},
        }},
        {"CALL/RET", "test_call_ret.bin", {
            {0x0200, 0x0007, "near CALL/RET"},
            {0x0202, 0x1234, "PUSH/POP"},
            {0x0204, 0x000A, "nested CALL"},
            {0x0206, 0xBEEF, "PUSH/POP cross"},
        }},
        {"Jumps/Loops", "test_jumps.bin", {
            {0x0200, 0x0001, "JE"},
            {0x0202, 0x0001, "JNE"},
            {0x0204, 0x0001, "JL"},
            {0x0206, 0x0001, "JG"},
            {0x0208, 0x0001, "JB"},
            {0x020A, 0x0001, "JA"},
            {0x020C, 0x0005, "LOOP count"},
            {0x020E, 0x0037, "LOOP sum"},
        }},
    };

    // --- Wiring (permanent) ---
    Signal vcc{"+5V"}, gnd{"GND"}, clk{"CLK"}, reset{"RESET"};
    Signal ready{"READY"}, nmi{"NMI"}, intr{"INTR"}, test_pin{"~TEST"};
    Signal cpu_lock{"~LOCK"}, rqgt0{"~RQ/GT0"};
    Signal qs0{"QS0"}, qs1{"QS1"}, s0{"~S0"}, s1{"~S1"}, s2{"~S2"};
    Bus ad{"AD", 8};
    Bus a_upper{"A", 12};

    Socket cpu_socket{"U3", "8088", 40};
    cpu_socket.wire(1, gnd); cpu_socket.wire(20, gnd);
    cpu_socket.wire(31, vcc); cpu_socket.wire(40, vcc);
    for (int i = 0; i < 8; ++i) cpu_socket.wire(16 - i, ad[i]);
    for (int i = 0; i < 7; ++i) cpu_socket.wire(8 - i, a_upper[i]);
    for (int i = 0; i < 5; ++i) cpu_socket.wire(39 - i, a_upper[7 + i]);
    cpu_socket.wire(19, clk); cpu_socket.wire(21, reset);
    cpu_socket.wire(22, ready); cpu_socket.wire(17, nmi);
    cpu_socket.wire(18, intr); cpu_socket.wire(23, test_pin);
    cpu_socket.wire(29, cpu_lock); cpu_socket.wire(30, rqgt0);
    cpu_socket.wire(24, qs1); cpu_socket.wire(25, qs0);
    cpu_socket.wire(26, s0); cpu_socket.wire(27, s1); cpu_socket.wire(28, s2);

    auto* cpu = cpu_socket.emplace<IC_8088>(0xF000, 0x0123);

    Signal* ad_ptrs[8];
    Signal* a_upper_ptrs[12];
    for (int i = 0; i < 8; ++i)  ad_ptrs[i] = &ad[i];
    for (int i = 0; i < 12; ++i) a_upper_ptrs[i] = &a_upper[i];

    auto bus_ptr = std::make_unique<BusGlue>();
    auto& bus = *bus_ptr;
    bus.ad = ad_ptrs;
    bus.a_upper = a_upper_ptrs;
    bus.s0 = &s0;
    bus.s1 = &s1;
    bus.s2 = &s2;

    TestClock clk_ic(clk, bus);

    // --- Run tests (power cycle between each) ---
    int passed = 0, failed = 0;

    for (auto& tc : tests) {
        spdlog::info("--- {} ---", tc.name);

        // Reset memory and BusGlue state
        std::memset(bus.mem, 0xF4, sizeof(bus.mem));
        bus.reset();

        // Load binary
        std::string path = std::string(ASM_TEST_DIR) + "/" + tc.bin_file;
        if (!load_bin(path, bus.mem, 0xF0123)) { ++failed; continue; }

        // Verify load
        spdlog::debug("  mem[F0123..F012A] = {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
            bus.mem[0xF0123], bus.mem[0xF0124], bus.mem[0xF0125], bus.mem[0xF0126],
            bus.mem[0xF0127], bus.mem[0xF0128], bus.mem[0xF0129], bus.mem[0xF012A]);

        // Power on
        ready.drive(Level::High);
        s0.drive(Level::High);
        s1.drive(Level::High);
        s2.drive(Level::High);
        vcc.drive(Level::High);
        clk_ic.power_on();
        cpu->power_on();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Power off
        vcc.drive(Level::HiZ);
        cpu->power_off();
        clk_ic.power_off();

        // Check results
        bool pass = true;
        for (auto& e : tc.expects) {
            uint16_t actual = bus.mem[e.addr] | (bus.mem[e.addr + 1] << 8);
            if (actual != e.value) {
                spdlog::error("  FAIL {}: [0x{:04X}] = 0x{:04X} (expected 0x{:04X})",
                    e.label, e.addr, actual, e.value);
                pass = false;
            } else {
                spdlog::info("  ok   {}: [0x{:04X}] = 0x{:04X}",
                    e.label, e.addr, actual);
            }
        }
        if (pass) ++passed; else ++failed;
    }

    spdlog::info("=== Results: {} passed, {} failed ===", passed, failed);
    return failed > 0 ? 1 : 0;
}
