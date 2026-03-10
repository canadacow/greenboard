// Minimal 8088 test bench.
//
// The CPU is real -- full pin-level, CLK-driven, threaded.
// Everything else is combinational: BusGlue emulates the 8288 + address
// latches + data transceiver + memory as instant logic. It runs
// synchronously inside TestClock's drive loop, BEFORE the CLK edge
// reaches the CPU's mailbox. This guarantees BusGlue sees the bus state
// and drives data before the CPU reads it.

#include "core/signal.h"
#include "core/component.h"
#include "board/socket.h"
#include "ic/ic_8088.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstring>
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

    // Per 8088 datasheet:
    //   Status active at T4(prev)/idle, stays active T1-T2, passive at T3.
    //   Address on AD0-7 during T1 (after CLK rise).
    //   ALE latches address at T1 CLK fall.
    //   Data on AD0-7 during T2-T3 (reads: memory drives; writes: CPU drives).
    //   CPU samples read data at T3 fall.
    //   CPU drives write data starting at T1 fall.

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

    // on_clk_rising: runs BEFORE CLK goes high.
    void on_clk_rising() {
        uint8_t status = decode_status();
        spdlog::debug("[BusGlue] CLK_RISE  state={} status={} bus_addr=0x{:05X} ad=0x{:02X}",
            tstate_name(), status, read_address(), read_ad());

        switch (t_state) {
        case TState::IDLE:
            if (status != 7) {
                t_state = TState::T1;
                cycle_type = status;
                spdlog::debug("[BusGlue]   -> T1 NEW CYCLE type={}", cycle_type);
            }
            break;

        case TState::T1:
            t_state = TState::T2;
            if (is_read_cycle()) {
                uint8_t val = mem[cycle_addr & 0xFFFFF];
                spdlog::debug("[BusGlue]   -> T2 READ drive mem[0x{:05X}]=0x{:02X}", cycle_addr, val);
                drive_ad(val);
            } else {
                spdlog::debug("[BusGlue]   -> T2");
            }
            break;

        case TState::T2:
            t_state = TState::T3;
            if (is_write_cycle()) {
                uint8_t val = read_ad();
                spdlog::debug("[BusGlue]   -> T3 WRITE mem[0x{:05X}]=0x{:02X}", cycle_addr, val);
                mem[cycle_addr & 0xFFFFF] = val;
            } else {
                spdlog::debug("[BusGlue]   -> T3");
            }
            break;

        case TState::T3:
            if (status == 7) {
                t_state = TState::T4;
                spdlog::debug("[BusGlue]   -> T4 (status passive)");
            } else {
                spdlog::debug("[BusGlue]   Tw (status={}, waiting)", status);
            }
            break;

        case TState::T4:
            if (is_read_cycle()) {
                release_ad();
            }
            // Per 8288: status goes active in T4 of prev cycle to start next.
            // Overlap T4/T1 so we don't burn an extra tick.
            if (status != 7) {
                t_state = TState::T1;
                cycle_type = status;
                spdlog::debug("[BusGlue]   -> T1 NEW CYCLE (overlapped T4) type={}", cycle_type);
            } else {
                t_state = TState::IDLE;
                spdlog::debug("[BusGlue]   -> IDLE");
            }
            break;
        }
    }

    // on_clk_falling: runs BEFORE CLK goes low.
    void on_clk_falling() {
        spdlog::debug("[BusGlue] CLK_FALL  state={} bus_addr=0x{:05X} ad=0x{:02X}",
            tstate_name(), read_address(), read_ad());
        if (t_state == TState::T1) {
            cycle_addr = read_address();
            spdlog::debug("[BusGlue]   ALE latch addr=0x{:05X}", cycle_addr);
        }
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

            spdlog::debug("[CLK] ---- tick {} ---- PRE-RISE", tick);
            bus_.on_clk_rising();
            spdlog::debug("[CLK] ---- tick {} ---- DRIVE HIGH", tick);
            clk_.drive(Level::High);

            Signal::wait_quiescent(stop);
            if (stop.stop_requested()) break;

            spdlog::debug("[CLK] ---- tick {} ---- PRE-FALL", tick);
            bus_.on_clk_falling();
            spdlog::debug("[CLK] ---- tick {} ---- DRIVE LOW", tick);
            clk_.drive(Level::Low);
            tick++;
        }
    }
    void on_signal_change() override {}
private:
    Signal& clk_;
    BusGlue& bus_;
};

int main() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("=== 8088 Test Bench ===");

    // Signals
    Signal vcc{"+5V"}, gnd{"GND"}, clk{"CLK"}, reset{"RESET"};
    Signal ready{"READY"}, nmi{"NMI"}, intr{"INTR"}, test_pin{"~TEST"};
    Signal cpu_lock{"~LOCK"}, rqgt0{"~RQ/GT0"};
    Signal qs0{"QS0"}, qs1{"QS1"}, s0{"~S0"}, s1{"~S1"}, s2{"~S2"};
    Bus ad{"AD", 8};
    Bus a_upper{"A", 12};

    // Wire CPU socket
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

    ready.drive(Level::High);
    s0.drive(Level::High);  // passive state (all High = status 7)
    s1.drive(Level::High);
    s2.drive(Level::High);
    auto* cpu = cpu_socket.emplace<IC_8088>(0xF000, 0x0123);

    // BusGlue (not a Component -- just a struct)
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
    std::memset(bus.mem, 0xF4, sizeof(bus.mem));

    // Load test program at F000:0123 = 0xF0123
    // MOV AX, 0x1234       (B8 34 12)
    // MOV [0x0200], AX     (A3 00 02)   -- DS=0 after reset
    // HLT                  (F4)
    uint8_t code[] = { 0xB8, 0x34, 0x12, 0xA3, 0x00, 0x02, 0xF4 };
    std::memcpy(bus.mem + 0xF0123, code, sizeof(code));

    // Clock (calls BusGlue synchronously)
    auto clk_ic = std::make_unique<TestClock>(clk, bus);

    // Power on
    vcc.drive(Level::High);
    clk_ic->power_on();
    cpu->power_on();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Power off
    vcc.drive(Level::HiZ);
    cpu->power_off();
    clk_ic->power_off();

    // Check
    uint16_t result = bus.mem[0x200] | (bus.mem[0x201] << 8);
    spdlog::info("[0x0200] = 0x{:04X} (expected 0x1234)", result);

    if (result == 0x1234) {
        spdlog::info("PASS");
        return 0;
    } else {
        spdlog::error("FAIL");
        return 1;
    }
}
