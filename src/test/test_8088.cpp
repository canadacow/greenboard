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
#include "ic/ic_8288.h"
#include "ic/ic_74s245.h"
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
// =========================================================================
// Minimal 8259A PIC emulation for the test harness.
// Handles ICW1-4 init sequence, IMR, IRR, ISR, INTA, and EOI.
// =========================================================================
struct PIC {
    enum class InitState { INIT_READY, EXPECT_ICW2, EXPECT_ICW3, EXPECT_ICW4 };
    InitState init_state = InitState::INIT_READY;
    uint8_t icw1 = 0;
    uint8_t vector_base = 0;  // ICW2: base interrupt vector (upper 5 bits)
    uint8_t icw4 = 0;
    uint8_t imr = 0xFF;       // interrupt mask register (all masked)
    uint8_t irr = 0;          // interrupt request register
    uint8_t isr = 0;          // in-service register
    bool read_isr = false;    // OCW3: read ISR instead of IRR
    Signal* intr = nullptr;   // INTR output to CPU

    void write(uint16_t port, uint8_t val) {
        if (port == 0x20) {
            if (val & 0x10) {
                // ICW1: bit 4 set
                icw1 = val;
                init_state = InitState::EXPECT_ICW2;
                imr = 0xFF;
                irr = 0;
                isr = 0;
                read_isr = false;
                spdlog::trace("[PIC] ICW1=0x{:02X}", val);
            } else if ((val & 0x18) == 0x00) {
                // OCW2: EOI commands (bits 4:3 = 00)
                uint8_t cmd = (val >> 5) & 7;
                if (cmd == 1) {
                    // Non-specific EOI: clear highest-priority ISR bit
                    for (int i = 0; i < 8; i++) {
                        if (isr & (1 << i)) { isr &= ~(1 << i); break; }
                    }
                    spdlog::trace("[PIC] non-specific EOI, ISR=0x{:02X}", isr);
                } else if (cmd == 3) {
                    // Specific EOI: clear bit specified by L2:L0
                    isr &= ~(1 << (val & 7));
                    spdlog::trace("[PIC] specific EOI IRQ{}, ISR=0x{:02X}", val & 7, isr);
                }
                update_intr();
            } else if ((val & 0x18) == 0x08) {
                // OCW3: bits 4:3 = 01
                if (val & 0x02) read_isr = (val & 0x01);
                spdlog::trace("[PIC] OCW3=0x{:02X} read_isr={}", val, read_isr);
            }
        } else { // port 0x21
            switch (init_state) {
            case InitState::EXPECT_ICW2:
                vector_base = val & 0xF8;
                spdlog::trace("[PIC] ICW2=0x{:02X} base_vector={}", val, vector_base);
                // ICW1 bit 1: 1=single, 0=cascade (need ICW3)
                init_state = (icw1 & 0x02) ?
                    ((icw1 & 0x01) ? InitState::EXPECT_ICW4 : InitState::INIT_READY) :
                    InitState::EXPECT_ICW3;
                break;
            case InitState::EXPECT_ICW3:
                spdlog::trace("[PIC] ICW3=0x{:02X}", val);
                init_state = (icw1 & 0x01) ? InitState::EXPECT_ICW4 : InitState::INIT_READY;
                break;
            case InitState::EXPECT_ICW4:
                icw4 = val;
                spdlog::trace("[PIC] ICW4=0x{:02X}", val);
                init_state = InitState::INIT_READY;
                break;
            case InitState::INIT_READY:
                // OCW1: interrupt mask register
                imr = val;
                spdlog::trace("[PIC] OCW1 IMR=0x{:02X}", val);
                update_intr();
                break;
            }
        }
    }

    uint8_t read(uint16_t port) {
        if (port == 0x20) {
            return read_isr ? isr : irr;
        } else { // port 0x21
            return imr;
        }
    }

    void raise_irq(int n) {
        irr |= (1 << n);
        spdlog::trace("[PIC] IRQ{} raised, IRR=0x{:02X}", n, irr);
        update_intr();
    }

    // INTA: acknowledge highest-priority pending interrupt
    uint8_t ack() {
        int irq = highest_priority();
        if (irq < 0) return vector_base; // shouldn't happen
        irr &= ~(1 << irq);
        isr |= (1 << irq);
        uint8_t vec = vector_base + irq;
        spdlog::trace("[PIC] INTA -> vector {} (IRQ{}), ISR=0x{:02X}", vec, irq, isr);
        // Auto-EOI: clear ISR immediately
        if (icw4 & 0x02) isr &= ~(1 << irq);
        update_intr();
        return vec;
    }

    int highest_priority() {
        uint8_t pending = irr & ~imr;
        for (int i = 0; i < 8; i++)
            if (pending & (1 << i)) return i;
        return -1;
    }

    bool has_interrupt() { return (irr & ~imr) != 0; }

    void update_intr() {
        if (!intr) return;
        if (has_interrupt())
            intr->drive(Level::High);
        else
            intr->drive(Level::Low);
    }

    void reset() {
        init_state = InitState::INIT_READY;
        icw1 = 0; vector_base = 0; icw4 = 0;
        imr = 0xFF; irr = 0; isr = 0;
        read_isr = false;
    }
};

struct BusGlue {
    Signal** ad;           // AD0-AD7 (8 pointers) -- address read only
    Signal** a_upper;      // A8-A19 (12 pointers)
    Signal** d;            // D0-D7 (8 pointers) -- system data bus (B side of 74S245)
    Signal* s0;
    Signal* s1;
    Signal* s2;
    uint8_t mem[1 << 20];
    uint8_t io[1 << 16];  // 64K I/O port space
    PIC pic;               // 8259A PIC at ports 0x20-0x21

    // T-state machine (mirrors 8288 bus controller)
    enum class TState { IDLE, T1, T2, T3, T4 };
    TState t_state = TState::IDLE;
    uint8_t cycle_type = 7;   // status code for current cycle
    uint32_t cycle_addr = 0;
    int bus_cycle_count = 0;   // debug: count completed bus cycles

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

    uint8_t read_d() {
        uint8_t val = 0;
        for (int i = 0; i < 8; ++i)
            if (d[i] && d[i]->level() == Level::High)
                val |= (1u << i);
        return val;
    }

    void drive_d(uint8_t val) {
        for (int i = 0; i < 8; ++i)
            if (d[i])
                d[i]->drive((val >> i) & 1 ? Level::High : Level::Low);
    }

    void release_d() {
        for (int i = 0; i < 8; ++i)
            if (d[i]) d[i]->release();
    }

    // Status decode: 0=INTA, 1=IOR, 2=IOW, 4=FETCH, 5=MEMR, 6=MEMW, 7=passive
    bool is_read_cycle()  { return cycle_type == 0 || cycle_type == 1 || cycle_type == 4 || cycle_type == 5; }
    bool is_write_cycle() { return cycle_type == 2 || cycle_type == 6; }
    bool is_io_cycle()    { return cycle_type == 1 || cycle_type == 2; }
    bool is_inta_cycle()  { return cycle_type == 0; }

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

    // I/O read: route PIC ports to PIC, everything else to generic io[]
    uint8_t io_read(uint16_t port) {
        if (port == 0x20 || port == 0x21)
            return pic.read(port);
        return io[port];
    }

    // I/O write: route PIC ports to PIC, trigger port to IRQ, else generic io[]
    void io_write(uint16_t port, uint8_t val) {
        if (port == 0x20 || port == 0x21) {
            pic.write(port, val);
        } else if (port == 0xF0) {
            // Test trigger port: each bit raises corresponding IRQ
            for (int i = 0; i < 8; i++)
                if (val & (1 << i)) pic.raise_irq(i);
        } else {
            io[port] = val;
        }
    }

    void on_clk_rising() {
        uint8_t status = decode_status();
        spdlog::trace("[BusGlue] CLK_RISE  state={} status={} bus_addr=0x{:05X} ad=0x{:02X}",
            tstate_name(), status, read_address(), read_d());

        switch (t_state) {
        case TState::IDLE:
            if (status != 7) {
                t_state = TState::T1;
                cycle_type = status;
                spdlog::trace("[BusGlue] IDLE->T1 type={} cycle#{}", cycle_type, bus_cycle_count);
            }
            break;

        case TState::T1:
            t_state = TState::T2;
            if (is_read_cycle()) {
                uint8_t val;
                if (is_inta_cycle()) {
                    val = pic.ack();
                    spdlog::trace("[BusGlue] T2 INTA vector=0x{:02X} cycle#{}", val, bus_cycle_count);
                } else if (is_io_cycle()) {
                    val = io_read(cycle_addr & 0xFFFF);
                    spdlog::trace("[BusGlue] T2 IO READ port=0x{:04X} data=0x{:02X} cycle#{}", cycle_addr, val, bus_cycle_count);
                } else {
                    val = mem[cycle_addr & 0xFFFFF];
                    spdlog::trace("[BusGlue] T2 READ addr=0x{:05X} data=0x{:02X} cycle#{}", cycle_addr, val, bus_cycle_count);
                }
                drive_d(val);
            }
            break;

        case TState::T2:
            t_state = TState::T3;
            if (is_write_cycle()) {
                uint8_t val = read_d();
                if (is_io_cycle()) {
                    spdlog::trace("[BusGlue]   -> T3 IO WRITE port[0x{:04X}]=0x{:02X}", cycle_addr, val);
                    io_write(cycle_addr & 0xFFFF, val);
                } else {
                    spdlog::trace("[BusGlue]   -> T3 WRITE mem[0x{:05X}]=0x{:02X}", cycle_addr, val);
                    mem[cycle_addr & 0xFFFFF] = val;
                }
            } else {
                spdlog::trace("[BusGlue]   -> T3");
            }
            break;

        case TState::T3:
            // T3 is always exactly one clock. Move to T4 unconditionally.
            // (Wait states are handled via READY, not status polling.)
            t_state = TState::T4;
            if (status == 7)
                spdlog::trace("[BusGlue]   -> T4 (status passive)");
            else
                spdlog::trace("[BusGlue]   -> T4 (status={}, forced)", status);
            break;

        case TState::T4:
            if (status != 7) {
                t_state = TState::T1;
                cycle_type = status;
                bus_cycle_count++;
                spdlog::trace("[BusGlue] T4->T1 overlap type={} cycle#{}", cycle_type, bus_cycle_count);
            } else {
                if (is_read_cycle()) {
                    release_d();
                }
                bus_cycle_count++;
                t_state = TState::IDLE;
                spdlog::trace("[BusGlue] T4->IDLE cycle#{}", bus_cycle_count);
            }
            break;
        }
    }

    void on_clk_falling() {
        spdlog::trace("[BusGlue] CLK_FALL  state={} bus_addr=0x{:05X} ad=0x{:02X}",
            tstate_name(), read_address(), read_d());
        if (t_state == TState::T1) {
            cycle_addr = read_address();
            spdlog::trace("[BusGlue]   ALE latch addr=0x{:05X}", cycle_addr);
        }
    }

    void reset() {
        t_state = TState::IDLE;
        cycle_type = 7;
        cycle_addr = 0;
        bus_cycle_count = 0;
        pic.reset();
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
    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== 8088 Test Bench ===");
    spdlog::info("ASM_TEST_DIR: {}", ASM_TEST_DIR);

    // Test table
    std::vector<TestCase> tests = {
        {"MOV/XCHG", "test_mov.bin", {
            {0x0500, 0x1234, "MOV imm16"},
            {0x0502, 0x5678, "MOV reg-reg"},
            {0x0504, 0x00AB, "MOV byte"},
            {0x0506, 0xDEF0, "XCHG ax"},
            {0x0508, 0x9ABC, "XCHG bx"},
        }},
        {"ALU", "test_alu.bin", {
            {0x0500, 0x0042, "ADD"},
            {0x0502, 0x0010, "SUB"},
            {0x0504, 0xFFBE, "NEG"},
            {0x0506, 0x1234, "AND"},
            {0x0508, 0xFFFF, "OR"},
            {0x050A, 0xEDCB, "XOR"},
            {0x050C, 0xEDCA, "NOT"},
            {0x050E, 0x2468, "SHL"},
            {0x0510, 0x048D, "SHR"},
            {0x0512, 0x0001, "CMP/JE"},
            {0x0514, 0x008A, "ADC"},
            {0x0516, 0x00FE, "SBB"},
        }},
        {"CALL/RET", "test_call_ret.bin", {
            {0x0500, 0x0007, "near CALL/RET"},
            {0x0502, 0x1234, "PUSH/POP"},
            {0x0504, 0x000A, "nested CALL"},
            {0x0506, 0xBEEF, "PUSH/POP cross"},
        }},
        {"Jumps/Loops", "test_jumps.bin", {
            {0x0500, 0x0001, "JE"},
            {0x0502, 0x0001, "JNE"},
            {0x0504, 0x0001, "JL"},
            {0x0506, 0x0001, "JG"},
            {0x0508, 0x0001, "JB"},
            {0x050A, 0x0001, "JA"},
            {0x050C, 0x0005, "LOOP count"},
            {0x050E, 0x0037, "LOOP sum"},
        }},
        {"Interrupts", "test_int.bin", {
            {0x0500, 0xAA55, "INT 0x40"},
            {0x0502, 0x0001, "INT 3 (breakpoint)"},
            {0x0504, 0x0001, "INTO (OF=1)"},
            {0x0506, 0x0000, "INTO (OF=0, skip)"},
            {0x0508, 0x0001, "IRET restores IF"},
            {0x050A, 0x0003, "nested INT"},
        }},
        {"Strings", "test_string.bin", {
            {0x0500, 0x0001, "REP MOVSB"},
            {0x0502, 0x0001, "REP STOSB"},
            {0x0504, 0x0044, "LODSB"},
            {0x0506, 0x0001, "REPNE SCASB found"},
            {0x0508, 0x0001, "SCASB position"},
            {0x050A, 0x0001, "REPE CMPSB"},
            {0x050C, 0x0001, "MOVSW"},
            {0x050E, 0x0001, "STD reverse"},
        }},
        {"MUL/IMUL/Shifts", "test_mul.bin", {
            {0x0500, 0x0048, "MUL byte"},
            {0x0502, 0x0000, "MUL byte hi"},
            {0x0504, 0x4000, "MUL word lo"},
            {0x0506, 0x0000, "MUL word hi"},
            {0x0508, 0xFFC8, "IMUL byte"},
            {0x050A, 0x0100, "MUL overflow"},
            {0x050C, 0x00A0, "SHL AL,CL"},
            {0x050E, 0x0003, "SHR AX,CL"},
            {0x0510, 0xFFFE, "SAR AX,1"},
            {0x0512, 0x0030, "SHL AX,CL"},
        }},
        {"BCD/Exotic", "test_bcd.bin", {
            {0x0500, 0x0042, "DAA"},
            {0x0502, 0x0022, "DAS"},
            {0x0504, 0x0105, "AAA"},
            {0x0506, 0x0035, "AAD"},
            {0x0508, 0x0305, "AAM"},
            {0x050A, 0x00A0, "ROL"},
            {0x050C, 0x0028, "ROR"},
            {0x050E, 0x0001, "LAHF/SAHF"},
            {0x0510, 0x0055, "XLAT"},
            {0x0512, 0x0001, "STC/CLC/CMC"},
            {0x0514, 0x1234, "LEA"},
            {0x0516, 0x0001, "LDS"},
        }},
        {"FAR CALL", "test_farcall.bin", {
            {0x0500, 0x0001, "CALL FAR imm"},
            {0x0502, 0x0001, "RETF"},
            {0x0504, 0x0001, "CALL FAR indirect"},
            {0x0506, 0x0001, "RETF imm16"},
            {0x0508, 0x0001, "JMP FAR imm"},
            {0x050A, 0x2000, "CS after far call"},
        }},
        {"I/O (PIC ports)", "test_io.bin", {
            {0x0500, 0x00AB, "OUT imm8 / IN imm8 byte"},
            {0x0502, 0x00CD, "OUT DX / IN DX byte"},
            {0x0504, 0xBEEF, "OUT/IN word"},
            {0x0506, 0x00FE, "PIC IMR readback"},
            {0x0508, 0x0001, "Timer IRQ0 -> INT 8"},
            {0x050A, 0x0008, "INT 8 vector correct"},
            {0x050C, 0x0001, "EOI clears ISR"},
            {0x050E, 0x0001, "I/O doesn't touch memory"},
        }},
        {"DIV/IDIV", "test_div.bin", {
            {0x0500, 0x0003, "DIV byte quot"},
            {0x0502, 0x0001, "DIV byte rem"},
            {0x0504, 0x000A, "DIV word quot"},
            {0x0506, 0x0000, "DIV word rem"},
            {0x0508, 0xFFFD, "IDIV byte quot"},
            {0x050A, 0xFFFF, "IDIV byte rem"},
            {0x050C, 0x0001, "DIV by zero"},
            {0x050E, 0x0001, "DIV overflow"},
        }},
    };

    // --- Wiring (permanent -- these are the copper traces on the test board) ---
    Signal vcc{"+5V"}, gnd{"GND"}, clk{"CLK"}, reset{"RESET"};
    Signal ready{"READY"}, nmi{"NMI"}, intr{"INTR"}, test_pin{"~TEST"};
    Signal cpu_lock{"~LOCK"}, rqgt0{"~RQ/GT0"};
    Signal qs0{"QS0"}, qs1{"QS1"}, s0{"~S0"}, s1{"~S1"}, s2{"~S2"};
    Bus ad{"AD", 8};
    Bus a_upper{"A", 12};

    // 8288 bus controller output signals
    Signal ale{"ALE"}, den{"~DEN"}, dtr{"DT/~R"};
    Signal memr{"~MEMR"}, memw{"~MEMW"};
    Signal ior_sig{"~IOR"}, iow_sig{"~IOW"}, inta_sig{"~INTA"};

    // System data bus (B side of 74S245 transceiver)
    Signal d0("D0"), d1("D1"), d2("D2"), d3("D3");
    Signal d4("D4"), d5("D5"), d6("D6"), d7("D7");
    Signal* d_arr[] = {&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7};

    // All traces on this test board. On power loss, every trace discharges.
    std::vector<Signal*> all_traces = {
        &vcc, &clk, &reset, &ready, &nmi, &intr, &test_pin,
        &cpu_lock, &rqgt0, &qs0, &qs1, &s0, &s1, &s2,
        &ale, &den, &dtr, &memr, &memw, &ior_sig, &iow_sig, &inta_sig,
    };
    for (int i = 0; i < 8; ++i)  all_traces.push_back(&ad[i]);
    for (int i = 0; i < 12; ++i) all_traces.push_back(&a_upper[i]);
    for (int i = 0; i < 8; ++i)  all_traces.push_back(d_arr[i]);

    // U3: 8088 CPU
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

    // U6: 8288 Bus Controller
    Socket bc_socket{"U6", "8288", 20};
    bc_socket.wire(1, gnd);
    bc_socket.wire(2, clk);
    bc_socket.wire(3, s1);    // ~S1
    bc_socket.wire(4, den);   // ~DEN output
    bc_socket.wire(5, ale);   // ALE output
    bc_socket.wire(6, vcc);   // CEN = always enabled
    bc_socket.wire(7, memr);  // ~MEMR output
    bc_socket.wire(8, memw);  // ~MEMW output
    bc_socket.wire(12, iow_sig);  // ~IOW output
    bc_socket.wire(13, ior_sig);  // ~IOR output
    bc_socket.wire(14, inta_sig); // ~INTA output
    bc_socket.wire(15, vcc);  // ~AEN = High (no DMA)
    bc_socket.wire(16, dtr);  // DT/~R output
    bc_socket.wire(18, s2);   // ~S2
    bc_socket.wire(19, s0);   // ~S0
    bc_socket.wire(20, vcc);
    auto* bc = bc_socket.emplace<IC_8288>();

    // U8: 74S245 Data Bus Transceiver
    // A side (pins 2-9) = AD7..AD0 (CPU local bus, reversed per BRD)
    // B side (pins 18-11) = D7..D0 (system data bus, reversed per BRD)
    Socket xcvr_socket{"U8", "74S245", 20};
    xcvr_socket.wire(1, den);  // ~G = ~DEN
    xcvr_socket.wire(2, ad[7]); xcvr_socket.wire(3, ad[6]);
    xcvr_socket.wire(4, ad[5]); xcvr_socket.wire(5, ad[4]);
    xcvr_socket.wire(6, ad[3]); xcvr_socket.wire(7, ad[2]);
    xcvr_socket.wire(8, ad[1]); xcvr_socket.wire(9, ad[0]);
    xcvr_socket.wire(10, gnd);
    xcvr_socket.wire(11, d0); xcvr_socket.wire(12, d1);
    xcvr_socket.wire(13, d2); xcvr_socket.wire(14, d3);
    xcvr_socket.wire(15, d4); xcvr_socket.wire(16, d5);
    xcvr_socket.wire(17, d6); xcvr_socket.wire(18, d7);
    xcvr_socket.wire(19, dtr);  // DIR = DT/~R
    xcvr_socket.wire(20, vcc);
    auto* xcvr = xcvr_socket.emplace<IC_74S245>();

    Signal* ad_ptrs[8];
    Signal* a_upper_ptrs[12];
    Signal* d_ptrs[8];
    for (int i = 0; i < 8; ++i)  ad_ptrs[i] = &ad[i];
    for (int i = 0; i < 12; ++i) a_upper_ptrs[i] = &a_upper[i];
    for (int i = 0; i < 8; ++i)  d_ptrs[i] = d_arr[i];

    auto bus_ptr = std::make_unique<BusGlue>();
    auto& bus = *bus_ptr;
    bus.ad = ad_ptrs;
    bus.a_upper = a_upper_ptrs;
    bus.d = d_ptrs;
    bus.s0 = &s0;
    bus.s1 = &s1;
    bus.s2 = &s2;
    bus.pic.intr = &intr;

    TestClock clk_ic(clk, bus);

    // --- Run tests (power cycle between each) ---
    int passed = 0, failed = 0;

    for (auto& tc : tests) {
        spdlog::info("--- {} ---", tc.name);

        // Reset memory, I/O space, and BusGlue state
        std::memset(bus.mem, 0xF4, sizeof(bus.mem));
        std::memset(bus.io, 0xFF, sizeof(bus.io));
        bus.reset();

        // Load binary
        std::string path = std::string(ASM_TEST_DIR) + "/" + tc.bin_file;
        if (!load_bin(path, bus.mem, 0xF0123)) { ++failed; continue; }

        // Verify load
        spdlog::trace("  mem[F0123..F012A] = {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
            bus.mem[0xF0123], bus.mem[0xF0124], bus.mem[0xF0125], bus.mem[0xF0126],
            bus.mem[0xF0127], bus.mem[0xF0128], bus.mem[0xF0129], bus.mem[0xF012A]);

        // Power on
        ready.drive(Level::High);
        s0.drive(Level::High);
        s1.drive(Level::High);
        s2.drive(Level::High);
        vcc.drive(Level::High);
        cpu->clear_halt();
        bc->power_on();
        xcvr->power_on();
        clk_ic.power_on();
        cpu->power_on();

        // Wait for CPU to halt, with 5s safety timeout
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!cpu->halted() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (!cpu->halted())
                spdlog::warn("  timeout -- CPU did not halt within 5s");
        }

        // Power off
        vcc.drive(Level::HiZ);
        cpu->power_off();
        clk_ic.power_off();
        bc->power_off();
        xcvr->power_off();

        // Power loss: every trace on the board discharges.
        for (auto* sig : all_traces)
            sig->reset();

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
