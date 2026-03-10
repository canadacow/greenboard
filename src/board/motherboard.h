#pragma once
#include "core/signal.h"
#include "board/socket.h"
#include "board/isa_slot.h"
#include "board/power_supply.h"
#include "board/passive.h"
#include <vector>
#include <memory>
#include <unordered_map>

namespace bench {

// The IBM PC 5150 motherboard -- 64KB-256KB "Model B".
//
// This class represents the physical PCB: every copper trace (Signal),
// every IC socket, every passive component, and every connector.
// Sockets are empty -- no ICs are installed. The wiring between
// socket pins and motherboard signals is fully modeled.
class Motherboard {
public:
    Motherboard();

    void power_on();
    void power_off();

    // =====================================================================
    // POWER SUPPLY
    // =====================================================================
    PowerSupply psu;

    // =====================================================================
    // CLOCK / OSCILLATOR
    // =====================================================================
    Signal osc{"OSC"};           // 14.31818 MHz crystal oscillator
    Signal clk{"CLK"};           // 4.77 MHz system clock (OSC/3, from 8284)
    Signal pclk{"PCLK"};         // 2.38 MHz peripheral clock (CLK/2, from 8284)
    Signal dclk{"DCLK"};         // DMA clock
    Signal clk88{"CLK88"};       // Clock to 8088 (from 8284)
    Signal osc_in{"OSC_IN"};     // Crystal input to 8284
    Signal ready{"READY"};       // READY to 8088 (from 8284)
    Signal rdy_wait{"RDY/WAIT"}; // Wait state input to 8284
    Signal rdy_nand{"RDY_NAND"}; // Output of 74LS30 (composite ready, active low = not ready)

    // =====================================================================
    // 8088 CPU LOCAL SIGNALS (before bus buffering)
    // =====================================================================
    Bus cpu_ad{"AD", 8};         // Multiplexed address/data (8088 AD0-AD7)
    Bus cpu_a{"A", 12};          // Address A8-A19 (8088 direct, A16-A19 muxed with status)
    Signal cpu_s0{"~S0"};        // Bus cycle status (to 8288)
    Signal cpu_s1{"~S1"};
    Signal cpu_s2{"~S2"};
    Signal cpu_rd{"~RD"};        // 8088 read strobe (active low) (max mode)
    Signal cpu_lock{"~LOCK"};    // Bus lock (to 8288)
    Signal cpu_rq_gt0{"~RQ/GT0"};  // Bus request/grant (8087 coprocessor)
    Signal cpu_rq_gt1{"~RQ/GT1"};
    Signal cpu_qs0{"QS0"};       // Queue status
    Signal cpu_qs1{"QS1"};
    Signal cpu_test{"~TEST"};    // Wait-for-test (from 8087 BUSY)
    Signal nmi{"NMI"};           // Non-maskable interrupt to 8088
    Signal intr{"INTR"};         // Maskable interrupt to 8088 (from 8259)
    Signal cpu_mn_mx{"MN/~MX"};  // Min/Max mode -- tied to GND (max mode)
    Signal cpu_reset{"RESET"};   // CPU reset (from 8284)

    // =====================================================================
    // 8288 BUS CONTROLLER OUTPUTS
    // =====================================================================
    Signal ale{"ALE"};           // Address latch enable
    Signal den{"~DEN"};          // Data enable (controls 74LS245 transceivers)
    Signal dt_r{"DT/~R"};        // Data transmit/receive direction
    Signal memr{"~MEMR"};        // Memory read
    Signal memw{"~MEMW"};        // Memory write
    Signal ior{"~IOR"};          // I/O read
    Signal iow{"~IOW"};          // I/O write
    Signal inta{"~INTA"};        // Interrupt acknowledge

    // =====================================================================
    // SYSTEM BUS (after address latches and data transceivers)
    // =====================================================================
    Bus sa{"SA", 20};            // System address bus (latched A0-A19)
    Bus sd{"SD", 8};             // System data bus (buffered D0-D7)

    // =====================================================================
    // 8259A PIC SIGNALS
    // =====================================================================
    Signal pic_cs{"~PIC_CS"};    // Chip select (active low, from address decode)
    Signal pic_int{"INT"};       // Interrupt output (directly drives INTR on CPU)
    Signal irq0{"IRQ0"};         // Timer (8253 ch0 OUT0)
    Signal irq1{"IRQ1"};         // Keyboard
    Signal irq2{"IRQ2"};         // Reserved / cascade
    Signal irq3{"IRQ3"};         // Serial port 2
    Signal irq4{"IRQ4"};         // Serial port 1
    Signal irq5{"IRQ5"};         // Hard disk
    Signal irq6{"IRQ6"};         // Floppy disk
    Signal irq7{"IRQ7"};         // Parallel port

    // =====================================================================
    // 8253 PIT SIGNALS
    // =====================================================================
    Signal pit_cs{"~PIT_CS"};
    Signal pit_clk0{"PIT_CLK0"}; // 1.193182 MHz (OSC/12)
    Signal pit_gate0{"PIT_GATE0"};
    Signal pit_out0{"PIT_OUT0"}; // -> IRQ0
    Signal pit_clk1{"PIT_CLK1"};
    Signal pit_gate1{"PIT_GATE1"};
    Signal pit_out1{"PIT_OUT1"}; // -> DMA ch0 DREQ (DRAM refresh)
    Signal pit_clk2{"PIT_CLK2"};
    Signal pit_gate2{"PIT_GATE2"}; // Controlled by PPI PB0
    Signal pit_out2{"PIT_OUT2"}; // -> speaker / cassette

    // =====================================================================
    // 8237A DMA CONTROLLER SIGNALS
    // =====================================================================
    Signal dma_cs{"~DMA_CS"};
    Signal hlda{"HLDA"};         // Hold acknowledge (from CPU via 8288)
    Signal hrq{"HRQ"};           // Hold request (to CPU)
    Signal dack0{"~DACK0"};      // DMA ch0 ack (DRAM refresh)
    Signal dack1{"~DACK1"};
    Signal dack2{"~DACK2"};
    Signal dack3{"~DACK3"};
    Signal dreq0{"DREQ0"};       // DMA ch0 request (from PIT ch1)
    Signal dreq1{"DREQ1"};
    Signal dreq2{"DREQ2"};
    Signal dreq3{"DREQ3"};
    Signal tc{"TC"};             // Terminal count
    Signal aen{"AEN"};           // Address enable (active during DMA)
    Bus dma_page{"DMA_PAGE", 4}; // Page register bits (from 74LS670)

    // =====================================================================
    // 8255A PPI SIGNALS
    // =====================================================================
    Signal ppi_cs{"~PPI_CS"};
    // Port A (PA0-PA7) -- reads DIP switch settings / keyboard data
    Bus ppi_pa{"PPI_PA", 8};
    // Port B (PB0-PB7) -- controls speaker, cassette motor, etc.
    Bus ppi_pb{"PPI_PB", 8};
    // Port C (PC0-PC7) -- status: RAM parity, I/O ch ck, timer out, etc.
    Bus ppi_pc{"PPI_PC", 8};

    // =====================================================================
    // KEYBOARD INTERFACE
    // =====================================================================
    Signal kbd_data{"KBD_DATA"};
    Signal kbd_clk{"KBD_CLK"};
    Signal kbd_reset{"KBD_RESET"};
    Bus kbd_scan{"KBD_SCAN", 8};  // Parallel scancode output from 74LS322 shift register

    // =====================================================================
    // SPEAKER / CASSETTE
    // =====================================================================
    Signal spkr_data{"SPKR_DATA"};  // AND of PIT OUT2 and PPI PB1
    Signal spkr_gate{"SPKR_GATE"};  // PPI PB0 -> PIT GATE2
    Signal cas_motor{"CAS_MOTOR"};   // PPI PB3 -> cassette motor relay
    Signal cas_data_out{"CAS_DATA_OUT"};
    Signal cas_data_in{"CAS_DATA_IN"};

    // =====================================================================
    // MEMORY CONTROL
    // =====================================================================
    Signal ras{"~RAS"};          // Row address strobe (to DRAM)
    Signal cas_0{"~CAS0"};       // Column address strobe bank 0
    Signal cas_1{"~CAS1"};       // Column address strobe bank 1
    Signal cas_2{"~CAS2"};       // Column address strobe bank 2
    Signal cas_3{"~CAS3"};       // Column address strobe bank 3
    Signal we{"~WE"};            // Write enable (to DRAM)
    Bus ma{"MA", 8};             // Multiplexed DRAM address (from address MUX)
    Signal parity_chk{"PCK"};    // Parity check error
    Signal ras_cas_sel{"RAS/~CAS"};  // Row/column select for address MUX (from delay line)
    Signal delay_out{"DELAY_OUT"};   // 100ns delay line output (DRAM timing)
    Signal nmi_mask{"NMI_MASK"};     // NMI mask register output (from I/O port 0A0h)

    // =====================================================================
    // ADDRESS DECODE / CHIP SELECTS
    // =====================================================================
    Signal rom_cs{"~ROM_CS"};    // BIOS ROM chip select
    Signal ram_cs{"~RAM_CS"};    // RAM chip select
    Signal io_cs{"~IO_CS"};      // I/O address space decode

    // =====================================================================
    // RESET
    // =====================================================================
    Signal reset_drv{"RESET_DRV"}; // Active reset (active high, derived from 8284 RESET)

    // =====================================================================
    // IC SOCKETS  (sourced from BRD: 64_256KB_SYSTEM_BOARD_rev1_2a)
    // =====================================================================
    // Part numbers match the BRD netlist exactly (74S = Schottky).

    // --- U1: MC1741 Op-Amp (8-pin DIP) -- speaker amplifier ---
    Socket u1 {"U1",  "MC1741",      8};

    // --- U2: Intel 8259A PIC (28-pin DIP) ---
    Socket u2 {"U2",  "8259A",      28};

    // --- U3: Intel 8088 CPU (40-pin DIP) ---
    Socket u3 {"U3",  "8088",       40};

    // --- XU4: Intel 8087 Math Coprocessor socket (40-pin DIP, optional/empty) ---
    // BRD ref is XU4 (auxiliary socket), not U4.
    Socket xu4{"XU4", "8087",       40};

    // --- U5: 74LS30 8-Input NAND (14-pin DIP) -- composite ready ---
    Socket u5 {"U5",  "74LS30",     14};

    // --- U6: Intel 8288 Bus Controller (20-pin DIP) ---
    Socket u6 {"U6",  "8288",       20};

    // --- U7: 74S373 Octal Latch (20-pin DIP) -- address latch ---
    Socket u7 {"U7",  "74S373",     20};

    // --- U8: 74S245 Octal Bus Transceiver (20-pin DIP) ---
    Socket u8 {"U8",  "74S245",     20};

    // --- U9: 74S373 Octal Latch (20-pin DIP) -- address latch ---
    Socket u9 {"U9",  "74S373",     20};

    // --- U10: 74S373 Octal Latch (20-pin DIP) -- address latch ---
    Socket u10{"U10", "74S373",     20};

    // --- U11: Intel 8284A Clock Generator (18-pin DIP) ---
    Socket u11{"U11", "8284A",      18};

    // --- U12: 74S245 Octal Bus Transceiver (20-pin DIP) ---
    Socket u12{"U12", "74S245",     20};

    // --- U13: 74S245 Octal Bus Transceiver (20-pin DIP) ---
    Socket u13{"U13", "74S245",     20};

    // --- U14: 74S245 Octal Bus Transceiver (20-pin DIP) ---
    Socket u14{"U14", "74S245",     20};

    // --- U15: 74S244 Octal Buffer (20-pin DIP) ---
    Socket u15{"U15", "74S244",     20};

    // --- U16: 74S244 Octal Buffer (20-pin DIP) ---
    Socket u16{"U16", "74S244",     20};

    // --- U17: 74S244 Octal Buffer (20-pin DIP) ---
    Socket u17{"U17", "74S244",     20};

    // --- U18: 74S373 Octal Latch (20-pin DIP) -- DMA page latch ---
    Socket u18{"U18", "74S373",     20};

    // --- U19: L670 (74LS670) 4x4 Register File (16-pin DIP) -- DMA page register ---
    Socket u19{"U19", "L670",       16};

    // --- U23: 74S244 Octal Buffer (20-pin DIP) ---
    Socket u23{"U23", "74S244",     20};

    // --- U24: 74S322 Shift Register (20-pin DIP) -- keyboard ---
    Socket u24{"U24", "74S322",     20};

    // --- U26: 74S175 Quad D Flip-Flop (16-pin DIP) ---
    Socket u26{"U26", "74S175",     16};

    // --- U27: 74LS02 Quad NOR (14-pin DIP) ---
    Socket u27{"U27", "74LS02",     14};

    // --- U28-U33: 8K x 8 ROM Sockets (24-pin DIP) ---
    Socket u28{"U28", "8K_X_8ROS",  24};   // ROM (empty on base config)
    Socket u29{"U29", "8K_X_8ROS",  24};   // BASIC C1.10
    Socket u30{"U30", "8K_X_8ROS",  24};   // BASIC C1.10
    Socket u31{"U31", "8K_X_8ROS",  24};   // BASIC C1.10
    Socket u32{"U32", "8K_X_8ROS",  24};   // BASIC C1.10
    Socket u33{"U33", "8K_X_8ROS",  24};   // BIOS (FE000-FFFFF)

    // --- U34: Intel 8253-5 PIT (24-pin DIP) ---
    Socket u34{"U34", "8253-5",     24};

    // --- U35: Intel 8237A DMA Controller (40-pin DIP) ---
    Socket u35{"U35", "8237A",      40};

    // --- U36: Intel 8255A-5 PPI (40-pin DIP) ---
    Socket u36{"U36", "8255A-5",    40};

    // --- DRAM Bank 0: U37-U45 (9 x 16-pin DIP, RAM_64K_X_1) ---
    std::vector<Socket> ram_bank0;  // U37-U45

    // --- U46: 74S138 3-to-8 Decoder (16-pin DIP) ---
    Socket u46{"U46", "74S138",     16};

    // --- U47: 74S138 3-to-8 Decoder (16-pin DIP) ---
    Socket u47{"U47", "74S138",     16};

    // --- U48: 74S138 3-to-8 Decoder (16-pin DIP) ---
    Socket u48{"U48", "74S138",     16};

    // --- U49: 74S08 Quad AND, Schottky (14-pin DIP) ---
    Socket u49{"U49", "74S08",      14};

    // --- U50: 74S02 Quad NOR, Schottky (14-pin DIP) ---
    Socket u50{"U50", "74S02",      14};

    // --- U51: 74S04 Hex Inverter, Schottky (14-pin DIP) ---
    Socket u51{"U51", "74S04",      14};

    // --- U52: 74S00 Quad NAND, Schottky (14-pin DIP) ---
    Socket u52{"U52", "74S00",      14};

    // --- DRAM Bank 1: U53-U61 (9 x 16-pin DIP, empty on base config) ---
    std::vector<Socket> ram_bank1;  // U53-U61

    // --- U62: 74S158 Quad 2-to-1 MUX, Schottky (16-pin DIP) -- DRAM addr MUX ---
    Socket u62{"U62", "74S158",     16};

    // --- U63: 74S38 Quad OC NAND, Schottky (14-pin DIP) ---
    Socket u63{"U63", "74S38",      14};

    // --- U64: 74S20 Dual 4-Input NAND, Schottky (14-pin DIP) ---
    Socket u64{"U64", "74S20",      14};

    // --- U65: 74S138 3-to-8 Decoder, Schottky (16-pin DIP) ---
    Socket u65{"U65", "74S138",     16};

    // --- U66: 74S138 3-to-8 Decoder, Schottky (16-pin DIP) ---
    Socket u66{"U66", "74S138",     16};

    // --- U67: 74S74 Dual D Flip-Flop, Schottky (14-pin DIP) ---
    Socket u67{"U67", "74S74",      14};

    // --- DRAM Bank 2: U69-U77 (9 x 16-pin DIP, empty on base config) ---
    std::vector<Socket> ram_bank2;  // U69-U77

    // --- U79: 74S158 Quad 2-to-1 MUX, Schottky (16-pin DIP) -- DRAM addr MUX ---
    Socket u79{"U79", "74S158",     16};

    // --- U80: 74S125 Quad Tri-State Buffer, Schottky (14-pin DIP) ---
    Socket u80{"U80", "74S125",     14};

    // --- U81: 74S00 Quad NAND, Schottky (14-pin DIP) -- RAS/CAS timing ---
    Socket u81{"U81", "74S00",      14};

    // --- U82: 74S74 Dual D Flip-Flop, Schottky (14-pin DIP) ---
    Socket u82{"U82", "74S74",      14};

    // --- U83: 74S04 Hex Inverter, Schottky (14-pin DIP) ---
    Socket u83{"U83", "74S04",      14};

    // --- U84: 74S10 Triple 3-Input NAND, Schottky (14-pin DIP) ---
    Socket u84{"U84", "74S10",      14};

    // --- DRAM Bank 3: U85-U93 (9 x 16-pin DIP, empty on base config) ---
    std::vector<Socket> ram_bank3;  // U85-U93

    // --- U94: 74S280 9-Bit Parity Generator/Checker (14-pin DIP) ---
    Socket u94{"U94", "74S280",     14};

    // --- U95: 75477 Dual Peripheral AND Driver (8-pin DIP) -- speaker ---
    Socket u95{"U95", "75477",       8};

    // --- U96: 74S74 Dual D Flip-Flop, Schottky (14-pin DIP) ---
    Socket u96{"U96", "74S74",      14};

    // --- U97: 74S08 Quad AND, Schottky (14-pin DIP) ---
    Socket u97{"U97", "74S08",      14};

    // --- U98: 74S175 Quad D Flip-Flop, Schottky (16-pin DIP) ---
    Socket u98{"U98", "74S175",     16};

    // --- U99: 74S04 Hex Inverter, Schottky (14-pin DIP) ---
    Socket u99{"U99", "74S04",      14};

    // --- U100: Empty Socket (20-pin DIP, unpopulated on stock board) ---
    Socket u100{"U100", "20DIP300", 20};

    // --- U101: 74LS32 Quad OR Gate (14-pin DIP) ---
    Socket u101{"U101", "74LS32",   14};

    // =====================================================================
    // DELAY LINES
    // =====================================================================

    // --- TD1: Delay Line (8-pin DIP) -- DRAM timing ---
    Socket td1{"TD1", "TIME_DELAY_2", 8};

    // --- TD2: Delay Line (3-pin) -- additional timing ---
    Socket td2{"TD2", "TIME_DELAY_1", 3};

    // =====================================================================
    // ISA EXPANSION SLOTS (5 slots on the 5150)
    // =====================================================================
    IsaSlot j1{"J1"};
    IsaSlot j2{"J2"};
    IsaSlot j3{"J3"};
    IsaSlot j4{"J4"};
    IsaSlot j5{"J5"};

    // =====================================================================
    // CONNECTORS (non-ISA)
    // =====================================================================
    Connector j6 {"J6", "CASSETTE",  8};   // 5-pin DIN cassette port
    Connector j7 {"J7", "KBD",       8};   // 5-pin DIN keyboard port
    Connector j8 {"J8", "+RUN",      2};   // 2-pin run/halt jumper header
    Connector p1 {"P1", "POWER_CON", 7};   // PSU connector 1
    Connector p2 {"P2", "POWER_CON", 7};   // PSU connector 2
    Connector p3 {"P3", "CONN_4",    4};   // 4-pin auxiliary header
    Connector p4 {"P4", "BERG_2X2",  4};   // 4-pin Berg strip (speaker)

    // =====================================================================
    // DIP SWITCHES (from BRD: SW1, SW2)
    // =====================================================================
    DipSwitch sw1{"SW1", 8};
    // SW1-1: OFF=no floppy, ON=1+ floppy
    // SW1-2: not used (8087 installed)
    // SW1-3,4: system board RAM (00=16K, 01=32K, 10=48K, 11=64K)
    // SW1-5,6: display (00=reserved, 01=CGA40, 10=CGA80, 11=MDA)
    // SW1-7,8: number of floppies (00=1, 01=2, 10=3, 11=4)

    DipSwitch sw2{"SW2", 8};
    // SW2-1..5: RAM expansion (varies by config)
    // SW2-6..8: not used on early 5150

    // =====================================================================
    // DISCRETE RESISTORS (from BRD: R1-R25)
    // =====================================================================
    Resistor r1  {"R1",  "18K"};
    Resistor r2  {"R2",  "1MEG"};
    Resistor r3  {"R3",  "18K"};
    Resistor r4  {"R4",  "18K"};
    Resistor r5  {"R5",  "18K"};
    Resistor r6  {"R6",  "150"};
    Resistor r7  {"R7",  "1200"};
    Resistor r8  {"R8",  "4.7K"};
    Resistor r9  {"R9",  "3.9K"};
    Resistor r10 {"R10", "33"};
    Resistor r11 {"R11", "180"};
    Resistor r12 {"R12", "220"};
    Resistor r13 {"R13", "27"};
    Resistor r14 {"R14", "27"};
    Resistor r15 {"R15", "27"};
    Resistor r16 {"R16", "27"};
    Resistor r17 {"R17", "27"};
    Resistor r18 {"R18", "27"};
    Resistor r19 {"R19", "27"};
    Resistor r20 {"R20", "27"};
    Resistor r21 {"R21", "27"};
    Resistor r22 {"R22", "510"};
    Resistor r23 {"R23", "30"};
    Resistor r25 {"R25", "510"};      // Note: no R24 on this board

    // =====================================================================
    // RESISTOR NETWORKS (from BRD: RN1-RN4, 16-pin DIP SIP packages)
    // =====================================================================
    ResistorNetwork rn1{"RN1", "4.7K",  16};
    ResistorNetwork rn2{"RN2", "8.2K",  16};
    ResistorNetwork rn3{"RN3", "4.7K",  16};
    ResistorNetwork rn4{"RN4", "30",    16};

    // =====================================================================
    // CAPACITORS (from BRD: C1-C48, bypass and coupling caps)
    // Role is classified at power-on from wiring (bypass/filter/coupling).
    // =====================================================================

    // --- Signal capacitors (filter / coupling / timing) ---
    // C1: VCC-GND bypass (3-pad footprint, all pads on power rails)
    Capacitor c1  {"C1",  ".047uF"};
    // C3: VCC-GND bypass (3-pad footprint, all pads on power rails)
    Capacitor c3  {"C3",  ".047uF"};
    // C5: Speaker filter -- N-000338 to GND
    //     Connects to U5 (74LS30, composite ready NAND) pin 5
    //     and P4 (speaker connector) pins 2/4.
    //     Forms low-pass filter on speaker output path.
    Capacitor c5  {"C5",  ".01uF"};
    // C8: Cassette motor relay RC timing -- N-000322 to N-000321
    //     Pin 1 (N-000322): connects to R5 (18K resistor) pin 2
    //     Pin 2 (N-000321): connects to K1 (cassette relay) pin 4
    //     Forms RC delay with R5 for relay de-bounce/timing.
    Capacitor c8  {"C8",  ".047uF"};
    // C9: Speaker driver filter -- N-000320 to GND
    //     Connects to R10 (33 ohm) pin 2 and U95 (75477 driver) pin 6.
    //     Forms RC filter on speaker driver output stage.
    Capacitor c9  {"C9",  ".01uF"};
    // Bypass caps (VCC-GND decoupling)
    Capacitor c22 {"C22", ".047uF"};
    Capacitor c23 {"C23", ".047uF"};
    Capacitor c24 {"C24", ".047uF"};
    Capacitor c25 {"C25", ".047uF"};
    Capacitor c26 {"C26", ".047uF"};
    Capacitor c27 {"C27", ".047uF"};
    Capacitor c28 {"C28", ".047uF"};
    Capacitor c29 {"C29", ".047uF"};
    Capacitor c30 {"C30", ".047uF"};
    Capacitor c31 {"C31", ".047uF"};
    Capacitor c32 {"C32", ".047uF"};
    Capacitor c33 {"C33", ".047uF"};
    Capacitor c34 {"C34", ".047uF"};
    Capacitor c35 {"C35", ".047uF"};
    Capacitor c36 {"C36", ".047uF"};
    Capacitor c37 {"C37", ".047uF"};
    Capacitor c38 {"C38", ".047uF"};
    Capacitor c39 {"C39", ".047uF"};
    Capacitor c40 {"C40", ".047uF"};
    Capacitor c41 {"C41", ".047uF"};
    Capacitor c42 {"C42", ".047uF"};
    Capacitor c43 {"C43", ".047uF"};
    Capacitor c44 {"C44", ".047uF"};
    Capacitor c45 {"C45", ".047uF"};
    Capacitor c46 {"C46", ".047uF"};
    Capacitor c47 {"C47", ".047uF"};
    Capacitor c48 {"C48", ".047uF"};

    // Unnamed bypass caps (10x generic "C1-1" in BRD, no unique ref designator)
    // These are bulk VCC-GND decoupling caps distributed across the board.
    std::vector<Capacitor> bypass_caps;  // 10 unnamed bypass caps

    // =====================================================================
    // CRYSTAL
    // =====================================================================
    // Y1: 14.31818 MHz crystal for 8284A clock generator.
    //     Pin 1 (osc_in):  N-000169 (to VC1 pin 2, trimmer)
    //     Pin 2 (osc_out): N-000211 (to U11 pin 16 = 8284A X1)
    //     Pins 3,4: GND (case ground)
    Crystal y1{"Y1", "14.31818MHz"};

    // =====================================================================
    // DIODE
    // =====================================================================
    // D1: Clamp diode on cassette data input.
    //     Anode = GND, cathode = CASS_DATA_IN (U36/8255A pin 13, R1 pin 1).
    //     Prevents cassette input voltage from going below ground.
    //     Role classified at power-on from wiring.
    Diode d1{"D1", "TYPE_FC"};

    // =====================================================================
    // RELAY (cassette motor control)
    // =====================================================================
    // K1: G5V-2 DPDT relay for cassette port motor and data switching.
    //     Pin 1:  +5V (coil power)
    //     Pin 16: N-000332 (coil drive from U95/75477 pin 3)
    //     Pin 4:  N-000321 (RC timing from C8/R5)
    //     Pin 6:  N-000335 (to R7, P4/speaker, R8)
    //     Pin 8:  N-000334 (to J6/cassette pin 4)
    //     Pin 9:  N-000333 (to J6/cassette pin 3)
    //     Pin 13: N-000331 (to J6/cassette pin 1)
    //     Coil energized by PPI PB3 via U95 driver.
    Relay k1{"K1", "G5V-2"};

    // =====================================================================
    // TRIMMER CAPACITOR
    // =====================================================================
    // VC1: Crystal oscillator tuning trimmer (5-30pF).
    //     Pin 1: N-000212 (U11/8284A pin 17 = TANK, R25 pin 2)
    //     Pin 2: N-000169 (Y1/crystal pin 1)
    //     Fine-tunes the 14.31818 MHz oscillator frequency.
    Trimmer vc1{"VC1", "5-30pF"};

    // =====================================================================
    // MOUNTING HOLES (H1-H9, no electrical function)
    // =====================================================================
    // Not modeled -- they have no nets or electrical significance.


    // =====================================================================
    // JUMPERS
    // =====================================================================
    std::vector<Jumper> jumpers;

private:
    void create_ram_sockets();
    void wire_from_brd();
    void build_net_map();
    void build_component_registry();

    // Net name -> Signal* lookup. Includes all named signals on the board
    // plus dynamically created signals for anonymous/internal nets.
    std::unordered_map<std::string, Signal*> net_map_;

    // Owns dynamically created Signal objects (for nets not declared as members).
    std::vector<std::unique_ptr<Signal>> dynamic_signals_;

    // Component registry: ref -> typed pointers for BRD wiring.
    std::unordered_map<std::string, Socket*> sockets_;
    std::unordered_map<std::string, IsaSlot*> isa_slots_;
    std::unordered_map<std::string, DipSwitch*> dip_switches_;
    std::unordered_map<std::string, Connector*> connectors_;
    std::unordered_map<std::string, Resistor*> resistors_;
    std::unordered_map<std::string, ResistorNetwork*> resistor_networks_;
    std::unordered_map<std::string, Capacitor*> capacitors_;
    std::unordered_map<std::string, Crystal*> crystals_;
    std::unordered_map<std::string, Diode*> diodes_;
    std::unordered_map<std::string, Relay*> relays_;
    std::unordered_map<std::string, Trimmer*> trimmers_;
};

} // namespace bench
