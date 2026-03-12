// 8088 CPU test bench -- data-driven instruction tests.
//
// The CPU is real -- full pin-level, CLK-driven, threaded.
// The 8284A clock generator is real -- drives OSC/CLK/PCLK/RESET/READY.
// The 8288 bus controller is real -- decodes S0-S2 into control signals.
// The 74S245 transceiver is real -- bidirectional data bus transfer.
// The 74S373 address latches are real -- ALE-triggered address capture (U7/U9/U10).
// The 74S138 I/O decoder is real -- U66 decodes XA5-7 into PIC/PIT/PPI/DMA chip selects.
// The 8259A PIC is real -- signal-level interrupt handling.
// BusGlue is a reactive Component: memory + generic I/O, subscribes to CLK.
//
// Each test is a flat binary assembled by NASM, loaded at F000:0123
// (physical 0xF0123). DS=SS=0 after reset. Results checked at 0x0200+.

#include "core/signal.h"
#include "core/threaded_component.h"
#include "core/scheduler.h"
#include "board/socket.h"
#include "ic/ic_8088.h"
#include "ic/ic_8288.h"
#include "ic/ic_74s245.h"
#include "ic/ic_8259a.h"
#include "ic/ic_8284a.h"
#include "ic/ic_74s373.h"
#include "ic/ic_74s138.h"
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
// BusGlue: reactive Component -- address decode + memory.
// Subscribes to CLK, detects edges, drives data/control for each T-state.
// Handles memory and generic I/O. PIC chip-select comes from U66 (74S138).
// =========================================================================
class BusGlue : public ThreadedComponent {
public:
    BusGlue() : ThreadedComponent("BusGlue") {}

    Signal** xa = nullptr;       // XA0-XA19 (20 pointers) -- latched address from 74S373s
    Signal** d = nullptr;        // D0-D7 (8 pointers) -- system data bus
    Signal* s0 = nullptr;
    Signal* s1 = nullptr;
    Signal* s2 = nullptr;
    Signal* pin_clk = nullptr;   // CLK input (subscribe to this)
    std::unique_ptr<uint8_t[]> mem = std::make_unique<uint8_t[]>(1 << 20);
    std::unique_ptr<uint8_t[]> io  = std::make_unique<uint8_t[]>(1 << 16);

    Signal* pic_ir[8] = {};       // IR0-IR7 (for test trigger port 0xF0)

    void subscribe_clk() {
        if (pin_clk) pin_clk->connect(this);
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
        clk_prev_ = Level::HiZ;
    }

protected:
    void on_signal_change() override {
        Level clk_cur = pin_clk ? pin_clk->level() : Level::HiZ;

        if (clk_cur == Level::High && clk_prev_ != Level::High)
            on_clk_rising();
        if (clk_cur == Level::Low && clk_prev_ != Level::Low)
            on_clk_falling();

        clk_prev_ = clk_cur;
    }

private:
    Level clk_prev_ = Level::HiZ;

    uint8_t decode_status() {
        uint8_t v2 = (s2->level() == Level::High) ? 1 : 0;
        uint8_t v1 = (s1->level() == Level::High) ? 1 : 0;
        uint8_t v0 = (s0->level() == Level::High) ? 1 : 0;
        return (v2 << 2) | (v1 << 1) | v0;
    }

    uint32_t read_address() {
        uint32_t addr = 0;
        for (int i = 0; i < 20; ++i)
            if (xa[i] && xa[i]->level() == Level::High)
                addr |= (1u << i);
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

    bool is_read_cycle()  { return cycle_type == 0 || cycle_type == 1 || cycle_type == 4 || cycle_type == 5; }
    bool is_write_cycle() { return cycle_type == 2 || cycle_type == 6; }
    bool is_io_cycle()    { return cycle_type == 1 || cycle_type == 2; }
    bool is_inta_cycle()  { return cycle_type == 0; }
    // PIC ports 0x20-0x21 are handled by U66 (74S138) -> PIC ~CS. BusGlue skips them.
    bool is_hw_decoded(uint32_t addr) { return (addr & 0xFFFF) >= 0x20 && (addr & 0xFFFF) <= 0x3F; }

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

        switch (t_state) {
        case TState::IDLE:
            if (status != 7) {
                t_state = TState::T1;
                cycle_type = status;
            }
            break;

        case TState::T1:
            // ALE falls this CLK rising (8288 T1->T2). Latches capture but
            // may not have settled yet (concurrent). Just advance state.
            t_state = TState::T2;
            break;

        case TState::T2:
            // Address is latched (ALE fell last CLK rise, settled by now).
            // Drive read data / capture write data.
            t_state = TState::T3;
            if (is_read_cycle()) {
                if (is_inta_cycle()) {
                } else if (is_io_cycle() && is_hw_decoded(cycle_addr)) {
                    // PIC (and other U66-decoded ports) -- handled by real hardware.
                } else if (is_io_cycle()) {
                    uint8_t val = io_read(cycle_addr & 0xFFFF);
                    drive_d(val);
                } else {
                    uint8_t val = mem[cycle_addr & 0xFFFFF];
                    drive_d(val);
                }
            } else if (is_write_cycle()) {
                if (is_io_cycle() && is_hw_decoded(cycle_addr)) {
                    // PIC (and other U66-decoded ports) -- handled by real hardware.
                } else if (is_io_cycle()) {
                    uint8_t val = read_d();
                    io_write(cycle_addr & 0xFFFF, val);
                } else {
                    uint8_t val = read_d();
                    mem[cycle_addr & 0xFFFFF] = val;
                }
            } else {
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
        if (t_state == TState::T2) {
            // ALE fell last CLK rise, latches settled. Read latched address.
            cycle_addr = read_address();
        }
    }
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
    spdlog::set_level(spdlog::level::trace);
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
        { "DOS INT 21h", "test_dos.bin", {
            {0x0500, 0x0005, "AH=02 char count"},
            {0x0502, 0x0048, "AH=02 first char 'H'"},
            {0x0504, 0x006F, "AH=02 last char 'o'"},
            {0x0506, 0x000D, "AH=09 string length"},
            {0x0508, 0x0048, "AH=09 first char 'H'"},
            {0x050A, 0x0021, "AH=09 last char '!'"},
            {0x050C, 0x002A, "AH=4C exit code 42"},
        } },
        {"IRQ (advanced)", "test_irq.bin", {
            {0x0500, 0x0001, "IRQ1 fires (INT 9)"},
            {0x0502, 0x0001, "Priority: IRQ0 first"},
            {0x0504, 0x0001, "Priority: IRQ1 second"},
            {0x0506, 0x0001, "Masked IRQ blocked"},
            {0x0508, 0x0001, "Specific EOI"},
            {0x050A, 0x0001, "Auto-EOI"},
            {0x050C, 0x0002, "Nested HW interrupts"},
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

    // U66 I/O decode outputs
    Signal dma_cs{"~DMA_CS"}, intr_cs{"~INTR_CS"}, pit_cs{"~PIT_CS"}, ppi_cs{"~PPI_CS"};
    Signal aen_bar{"~AEN"};  // No DMA in test bench, always High

    // System data bus (B side of 74S245 transceiver)
    Signal d0("D0"), d1("D1"), d2("D2"), d3("D3");
    Signal d4("D4"), d5("D5"), d6("D6"), d7("D7");
    Signal* d_arr[] = {&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7};

    // PIC A0 wired directly to latched address bit 0 (XA0) on real board.
    // ~CS comes from U66 ~Y1 (intr_cs) -- no manual BusGlue decode needed.

    // IRQ lines (BusGlue drives these via test trigger port 0xF0)
    Signal irq0{"IRQ0"}, irq1{"IRQ1"}, irq2{"IRQ2"}, irq3{"IRQ3"};
    Signal irq4{"IRQ4"}, irq5{"IRQ5"}, irq6{"IRQ6"}, irq7{"IRQ7"};
    Signal* irq_arr[] = {&irq0, &irq1, &irq2, &irq3, &irq4, &irq5, &irq6, &irq7};

    // 8284A signals
    Signal osc{"OSC"}, pclk{"PCLK"}, res{"RES"};

    // Latched address bus (outputs from 74S373 latches, active after ALE)
    Signal xa[20] = {
        Signal("XA0"),  Signal("XA1"),  Signal("XA2"),  Signal("XA3"),
        Signal("XA4"),  Signal("XA5"),  Signal("XA6"),  Signal("XA7"),
        Signal("XA8"),  Signal("XA9"),  Signal("XA10"), Signal("XA11"),
        Signal("XA12"), Signal("XA13"), Signal("XA14"), Signal("XA15"),
        Signal("XA16"), Signal("XA17"), Signal("XA18"), Signal("XA19"),
    };

    // All traces on this test board. On power loss, every trace discharges.
    std::vector<Signal*> all_traces = {
        &vcc, &clk, &reset, &ready, &nmi, &intr, &test_pin,
        &cpu_lock, &rqgt0, &qs0, &qs1, &s0, &s1, &s2,
        &ale, &den, &dtr, &memr, &memw, &ior_sig, &iow_sig, &inta_sig,
        &dma_cs, &intr_cs, &pit_cs, &ppi_cs, &aen_bar,
        &osc, &pclk, &res,
    };
    for (int i = 0; i < 8; ++i)  all_traces.push_back(&ad[i]);
    for (int i = 0; i < 12; ++i) all_traces.push_back(&a_upper[i]);
    for (int i = 0; i < 8; ++i)  all_traces.push_back(d_arr[i]);
    for (int i = 0; i < 8; ++i)  all_traces.push_back(irq_arr[i]);
    for (int i = 0; i < 20; ++i) all_traces.push_back(&xa[i]);

    // U11: 8284A Clock Generator
    Socket clk_socket{"U11", "8284A", 18};
    clk_socket.wire(2, pclk);    // PCLK output
    clk_socket.wire(5, ready);   // READY output
    clk_socket.wire(8, clk);     // CLK output
    clk_socket.wire(9, gnd);     // GND
    clk_socket.wire(10, reset);  // RESET output
    clk_socket.wire(11, res);    // RES input (PWR_GOOD)
    clk_socket.wire(12, osc);    // OSC output
    clk_socket.wire(18, vcc);    // VCC
    auto* clk_gen = clk_socket.emplace<IC_8284A>();

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
    bc_socket.wire(3, s1);
    bc_socket.wire(4, den);
    bc_socket.wire(5, ale);
    bc_socket.wire(6, vcc);    // CEN = always enabled
    bc_socket.wire(7, memr);
    bc_socket.wire(8, memw);
    bc_socket.wire(12, iow_sig);
    bc_socket.wire(13, ior_sig);
    bc_socket.wire(14, inta_sig);
    bc_socket.wire(15, vcc);   // ~AEN = High (no DMA)
    bc_socket.wire(16, dtr);
    bc_socket.wire(18, s2);
    bc_socket.wire(19, s0);
    bc_socket.wire(20, vcc);
    auto* bc = bc_socket.emplace<IC_8288>();

    // U8: 74S245 Data Bus Transceiver
    Socket xcvr_socket{"U8", "74S245", 20};
    xcvr_socket.wire(1, den);
    xcvr_socket.wire(2, ad[7]); xcvr_socket.wire(3, ad[6]);
    xcvr_socket.wire(4, ad[5]); xcvr_socket.wire(5, ad[4]);
    xcvr_socket.wire(6, ad[3]); xcvr_socket.wire(7, ad[2]);
    xcvr_socket.wire(8, ad[1]); xcvr_socket.wire(9, ad[0]);
    xcvr_socket.wire(10, gnd);
    xcvr_socket.wire(11, d0); xcvr_socket.wire(12, d1);
    xcvr_socket.wire(13, d2); xcvr_socket.wire(14, d3);
    xcvr_socket.wire(15, d4); xcvr_socket.wire(16, d5);
    xcvr_socket.wire(17, d6); xcvr_socket.wire(18, d7);
    xcvr_socket.wire(19, dtr);
    xcvr_socket.wire(20, vcc);
    auto* xcvr = xcvr_socket.emplace<IC_74S245>();

    // U10: 74S373 Address Latch (low byte: AD0-AD7 -> XA0-XA7)
    Socket latch_lo{"U10", "74S373", 20};
    latch_lo.wire(1, gnd);       // ~OE = always enabled
    latch_lo.wire(11, ale);      // LE = ALE
    latch_lo.wire(10, gnd);
    latch_lo.wire(20, vcc);
    // D inputs = AD0-AD7, Q outputs = XA0-XA7
    // D pins: 3,4,7,8,13,14,17,18  Q pins: 2,5,6,9,12,15,16,19
    latch_lo.wire(3, ad[0]); latch_lo.wire(2, xa[0]);
    latch_lo.wire(4, ad[1]); latch_lo.wire(5, xa[1]);
    latch_lo.wire(7, ad[2]); latch_lo.wire(6, xa[2]);
    latch_lo.wire(8, ad[3]); latch_lo.wire(9, xa[3]);
    latch_lo.wire(13, ad[4]); latch_lo.wire(12, xa[4]);
    latch_lo.wire(14, ad[5]); latch_lo.wire(15, xa[5]);
    latch_lo.wire(17, ad[6]); latch_lo.wire(16, xa[6]);
    latch_lo.wire(18, ad[7]); latch_lo.wire(19, xa[7]);
    auto* latch_lo_ic = latch_lo.emplace<IC_74S373>();

    // U9: 74S373 Address Latch (mid byte: A0-A7 -> XA8-XA15)
    Socket latch_mid{"U9", "74S373", 20};
    latch_mid.wire(1, gnd);      // ~OE = always enabled
    latch_mid.wire(11, ale);     // LE = ALE
    latch_mid.wire(10, gnd);
    latch_mid.wire(20, vcc);
    latch_mid.wire(3, a_upper[0]); latch_mid.wire(2, xa[8]);
    latch_mid.wire(4, a_upper[1]); latch_mid.wire(5, xa[9]);
    latch_mid.wire(7, a_upper[2]); latch_mid.wire(6, xa[10]);
    latch_mid.wire(8, a_upper[3]); latch_mid.wire(9, xa[11]);
    latch_mid.wire(13, a_upper[4]); latch_mid.wire(12, xa[12]);
    latch_mid.wire(14, a_upper[5]); latch_mid.wire(15, xa[13]);
    latch_mid.wire(17, a_upper[6]); latch_mid.wire(16, xa[14]);
    latch_mid.wire(18, a_upper[7]); latch_mid.wire(19, xa[15]);
    auto* latch_mid_ic = latch_mid.emplace<IC_74S373>();

    // U7: 74S373 Address Latch (high nibble: A8-A11 -> XA16-XA19)
    Socket latch_hi{"U7", "74S373", 20};
    latch_hi.wire(1, gnd);       // ~OE = always enabled
    latch_hi.wire(11, ale);      // LE = ALE
    latch_hi.wire(10, gnd);
    latch_hi.wire(20, vcc);
    latch_hi.wire(3, a_upper[8]);  latch_hi.wire(2, xa[16]);
    latch_hi.wire(4, a_upper[9]);  latch_hi.wire(5, xa[17]);
    latch_hi.wire(7, a_upper[10]); latch_hi.wire(6, xa[18]);
    latch_hi.wire(8, a_upper[11]); latch_hi.wire(9, xa[19]);
    // D4-D7 unused on U7 -- only 4 address bits (A16-A19)
    auto* latch_hi_ic = latch_hi.emplace<IC_74S373>();

    // U66: 74S138 I/O Address Decoder
    // Decodes XA5-XA7 when XA8=0, XA9=0, ~AEN=High (no DMA).
    // ~Y0=~DMA_CS (0x00), ~Y1=~INTR_CS (0x20), ~Y2=~PIT_CS (0x40), ~Y3=~PPI_CS (0x60)
    Socket io_decode{"U66", "74S138", 16};
    io_decode.wire(1, xa[5]);       // A = XA5
    io_decode.wire(2, xa[6]);       // B = XA6
    io_decode.wire(3, xa[7]);       // C = XA7
    io_decode.wire(4, xa[9]);       // ~G2A = XA9 (must be Low)
    io_decode.wire(5, xa[8]);       // ~G2B = XA8 (must be Low)
    io_decode.wire(6, aen_bar);     // G1 = ~AEN (High = CPU on bus)
    io_decode.wire(8, gnd);
    io_decode.wire(15, dma_cs);     // ~Y0 = ~DMA_CS (0x00-0x1F)
    io_decode.wire(14, intr_cs);    // ~Y1 = ~INTR_CS (0x20-0x3F)
    io_decode.wire(13, pit_cs);     // ~Y2 = ~PIT_CS (0x40-0x5F)
    io_decode.wire(12, ppi_cs);     // ~Y3 = ~PPI_CS (0x60-0x7F)
    io_decode.wire(16, vcc);
    auto* io_dec = io_decode.emplace<IC_74S138>();

    // U2: 8259A PIC
    Socket pic_socket{"U2", "8259A", 28};
    pic_socket.wire(1, intr_cs);
    pic_socket.wire(2, iow_sig);
    pic_socket.wire(3, ior_sig);
    pic_socket.wire(4, d7);  pic_socket.wire(5, d6);
    pic_socket.wire(6, d5);  pic_socket.wire(7, d4);
    pic_socket.wire(8, d3);  pic_socket.wire(9, d2);
    pic_socket.wire(10, d1); pic_socket.wire(11, d0);
    pic_socket.wire(14, gnd);
    pic_socket.wire(16, vcc);         // ~SP/~EN = VCC (master mode)
    pic_socket.wire(17, intr);        // INT -> CPU INTR
    pic_socket.wire(18, irq0); pic_socket.wire(19, irq1);
    pic_socket.wire(20, irq2); pic_socket.wire(21, irq3);
    pic_socket.wire(22, irq4); pic_socket.wire(23, irq5);
    pic_socket.wire(24, irq6); pic_socket.wire(25, irq7);
    pic_socket.wire(26, inta_sig);
    pic_socket.wire(27, xa[0]);
    pic_socket.wire(28, vcc);
    auto* pic = pic_socket.emplace<IC_8259A>();

    // BusGlue: reactive address decode + memory
    Signal* xa_ptrs[20];
    Signal* d_ptrs[8];
    for (int i = 0; i < 20; ++i) xa_ptrs[i] = &xa[i];
    for (int i = 0; i < 8; ++i)  d_ptrs[i] = d_arr[i];

    BusGlue bus;
    bus.xa = xa_ptrs;
    bus.d = d_ptrs;
    bus.s0 = &s0;
    bus.s1 = &s1;
    bus.s2 = &s2;
    bus.pin_clk = &clk;
    for (int i = 0; i < 8; ++i) bus.pic_ir[i] = irq_arr[i];
    bus.subscribe_clk();

    // Scheduler: central CLK-edge evaluator for inline ICs.
    Scheduler scheduler;
    Signal::set_scheduler(&scheduler);
    scheduler.register_inline(xcvr);
    scheduler.register_inline(latch_lo_ic);
    scheduler.register_inline(latch_mid_ic);
    scheduler.register_inline(latch_hi_ic);
    scheduler.register_inline(io_dec);
    clk_gen->set_scheduler(&scheduler);

    // --- Run tests (power cycle between each) ---
    int passed = 0, failed = 0;

    for (auto& tc : tests) {
        spdlog::info("--- {} ---", tc.name);

        // Reset memory, I/O space, and BusGlue state
        std::memset(bus.mem.get(), 0xF4, 1 << 20);
        std::memset(bus.io.get(), 0xFF, 1 << 16);
        bus.reset();

        // Load binary
        std::string path = std::string(ASM_TEST_DIR) + "/" + tc.bin_file;
        if (!load_bin(path, bus.mem.get(), 0xF0123)) { ++failed; continue; }

        // Verify load
        spdlog::trace("  mem[F0123..F012A] = {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
            bus.mem[0xF0123], bus.mem[0xF0124], bus.mem[0xF0125], bus.mem[0xF0126],
            bus.mem[0xF0127], bus.mem[0xF0128], bus.mem[0xF0129], bus.mem[0xF012A]);

        // Seat all ICs (threads start, block on wait_mailbox).
        cpu->clear_halt();
        pic->power_on();
        bc->power_on();
        xcvr->power_on();
        io_dec->power_on();
        latch_lo_ic->power_on();
        latch_mid_ic->power_on();
        latch_hi_ic->power_on();
        bus.power_on();
        clk_gen->power_on();
        cpu->power_on();
        spdlog::debug("All ICs seated, pending={}", Signal::pending.load());

        // Flip the switch.
        gnd.drive(Level::Low);
        s0.drive(Level::High);
        s1.drive(Level::High);
        s2.drive(Level::High);
        aen_bar.drive(Level::High);  // No DMA -- CPU always owns bus
        vcc.drive(Level::High);
        scheduler.evaluate();
        spdlog::debug("VCC driven High, pending={}", Signal::pending.load());

        // Brief delay for 8284A to start oscillating and assert RESET.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        spdlog::debug("After 5ms delay, pending={}", Signal::pending.load());

        // Drive RES (power good) -- 8284A deasserts RESET on next CLK fall.
        res.drive(Level::High);
        spdlog::debug("RES driven High, pending={}", Signal::pending.load());

        // Wait for CPU to halt, with 10s safety timeout
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!cpu->halted() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (!cpu->halted())
                spdlog::warn("  timeout -- CPU did not halt within 10s");
        }

        // Power off: drop VCC, ICs detect and exit.
        res.drive(Level::Low);
        vcc.drive(Level::HiZ);
        scheduler.evaluate();
        cpu->power_off();
        clk_gen->power_off();
        bus.power_off();
        bc->power_off();
        xcvr->power_off();
        io_dec->power_off();
        latch_lo_ic->power_off();
        latch_mid_ic->power_off();
        latch_hi_ic->power_off();
        pic->power_off();

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
