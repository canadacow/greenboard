#pragma once
// Test board wiring -- signals, sockets, IC emplacement.
// This is the "official motherboard schematic" for the test bench,
// minus BusGlue (which is test-bench-specific).
// Cut/pasted from test_8088.cpp main() for reuse.

#include "core/signal.h"
#include "core/scheduler.h"
#include "board/socket.h"
#include "ic/ic_8088.h"
#include "ic/ic_8288.h"
#include "ic/ic_74s245.h"
#include "ic/ic_8259a.h"
#include "ic/ic_8284a.h"
#include "ic/ic_74s373.h"
#include "ic/ic_74s138.h"
#include "ic/ic_74s20.h"
#include "ic/ic_rom_8k.h"
#include "ic/ic_dram_256k.h"
#include "ic/ic_74s158.h"
#include "ic/ic_74s00.h"
#include "ic/ic_74s00_u81.h"
#include "ic/ic_74s04.h"
#include "ic/ic_74s08.h"
#include "ic/ic_74s244.h"
#include "ic/ic_8237a.h"
#include "board/isa_slot.h"
#include <string>
#include <vector>

using namespace bench;

struct TestBoard {
    // --- Signals (copper traces) ---
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
    Signal d0{"D0"}, d1{"D1"}, d2{"D2"}, d3{"D3"};
    Signal d4{"D4"}, d5{"D5"}, d6{"D6"}, d7{"D7"};
    Signal* d_arr[8] = {&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7};

    // IRQ lines (BusGlue drives these via test trigger port 0xF0)
    Signal irq0{"IRQ0"}, irq1{"IRQ1"}, irq2{"IRQ2"}, irq3{"IRQ3"};
    Signal irq4{"IRQ4"}, irq5{"IRQ5"}, irq6{"IRQ6"}, irq7{"IRQ7"};
    Signal* irq_arr[8] = {&irq0, &irq1, &irq2, &irq3, &irq4, &irq5, &irq6, &irq7};

    // DRAM signals
    Signal dram_ma0{"MA0"}, dram_ma1{"MA1"}, dram_ma2{"MA2"}, dram_ma3{"MA3"};
    Signal dram_ma4{"MA4"}, dram_ma5{"MA5"}, dram_ma6{"MA6"}, dram_ma7{"MA7"};
    Signal* dram_ma_arr[8] = {&dram_ma0, &dram_ma1, &dram_ma2, &dram_ma3,
                              &dram_ma4, &dram_ma5, &dram_ma6, &dram_ma7};
    Signal ras{"RAS"};
    Signal cas{"~CAS"};
    Signal dram_we{"~WE_DRAM"};
    Signal u83_mid{"U83_1_2"};  // intermediate: U83 pin2 -> pin3
    Signal addr_sel{"ADDR_SEL"};
    Signal md0{"MD0"}, md1{"MD1"}, md2{"MD2"}, md3{"MD3"};
    Signal md4{"MD4"}, md5{"MD5"}, md6{"MD6"}, md7{"MD7"};
    Signal mdp{"MDP"};
    Signal* md_arr[8] = {&md0, &md1, &md2, &md3, &md4, &md5, &md6, &md7};

    // DRAM decode chain signals
    Signal ram_addr_sel{"~RAM_ADDR_SEL"};
    Signal refrsh_gate{"~REFRSH_GATE"};
    Signal bank_sel_y4{"N-000256"}, bank_sel_y5{"N-000251"};
    Signal bank_sel_y6{"N-000255"}, bank_sel_y7{"N-000252"};
    Signal ras0{"~RAS0"}, ras1{"~RAS1"}, ras2{"~RAS2"}, ras3{"~RAS3"};
    Signal* ras_arr[4] = {&ras0, &ras1, &ras2, &ras3};
    Signal dram_cas0{"~CAS0"}, dram_cas1{"~CAS1"}, dram_cas2{"~CAS2"}, dram_cas3{"~CAS3"};
    Signal* dram_cas_arr[4] = {&dram_cas0, &dram_cas1, &dram_cas2, &dram_cas3};

    // DMA controller signals
    Signal holda{"HOLDA"};              // Hold acknowledge (undriven = DMA never gets bus)
    Signal hrq{"HRQ"};                  // Hold request (from 8237A)
    Signal adstb{"ADSTB"};              // Address strobe (for DMA page latch)
    Signal dma_aen_out{"DMA_AEN"};      // AEN output from 8237A
    Signal drq0{"DRQ0"};               // DMA request 0 (DRAM refresh, undriven)
    Signal drq1{"DRQ1"}, drq2{"DRQ2"}, drq3{"DRQ3"};  // ISA DMA requests
    Signal dack0_brd{"~DACK_0_BRD"};   // ~DACK0 from 8237A
    Signal dack1{"~DACK1"}, dack2{"~DACK2"}, dack3{"~DACK3"};
    Signal eop{"~EOP"};                 // End of process / terminal count

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

    // ROM-specific signals
    Signal rom_addr_sel{"~ROM_ADDR_SEL"};
    Signal cs7{"~CS7"};

    // ISA bus -- slots wired directly to motherboard signals (no buffer ICs).
    // U14/U15/U16/U17 (ISA bus buffers) are not instantiated in the test bench
    // because they require DMA direction control; series termination resistors
    // are aliased, so ISA-side and motherboard-side nets are the same.
    IsaSlot isa_slots[5] = {
        IsaSlot("J1"), IsaSlot("J2"), IsaSlot("J3"), IsaSlot("J4"), IsaSlot("J5"),
    };

    // All traces on this test board. On power loss, every trace discharges.
    std::vector<Signal*> all_traces;

    // --- Sockets ---
    Socket clk_socket{"U11", "8284A", 18};
    Socket cpu_socket{"U3", "8088", 40};
    Socket bc_socket{"U6", "8288", 20};
    Socket xcvr_socket{"U8", "74S245", 20};
    Socket mem_xcvr_socket{"U12", "74S245", 20};
    Socket latch_lo{"U10", "74S373", 20};
    Socket latch_mid{"U9", "74S373", 20};
    Socket latch_hi{"U7", "74S373", 20};
    Socket io_decode{"U66", "74S138", 16};
    Socket pic_socket{"U2", "8259A", 28};
    Socket nand_socket{"U64", "74S20", 14};
    Socket rom_decode{"U46", "74S138", 16};
    Socket rom_socket{"U33", "8K_X_8ROS", 24};
    Socket nand81_socket{"U81", "74S00", 14};
    Socket ram_range{"U48", "74S138", 16};
    Socket ras_decode{"U65", "74S138", 16};
    Socket ras_gate{"U49", "74S08", 14};
    Socket cas_decode{"U47", "74S138", 16};
    Socket mux_lo{"U62", "74S158", 16};
    Socket mux_hi{"U79", "74S158", 16};
    Socket inv_socket{"U83", "74S04", 14};
    Socket dma_socket{"U35", "8237A", 40};
    std::vector<Socket> ram_bank0, ram_bank1, ram_bank2, ram_bank3;

    // --- IC pointers (set by wire()) ---
    IC_8284A* clk_gen = nullptr;
    IC_8088* cpu = nullptr;
    IC_8288* bc = nullptr;
    IC_74S245* xcvr = nullptr;
    IC_74S245* mem_xcvr = nullptr;
    IC_74S373* latch_lo_ic = nullptr;
    IC_74S373* latch_mid_ic = nullptr;
    IC_74S373* latch_hi_ic = nullptr;
    IC_74S138* io_dec = nullptr;
    IC_8259A* pic = nullptr;
    IC_74S20* nand_ic = nullptr;
    IC_74S138* rom_dec = nullptr;
    IC_ROM_8K* rom_ic = nullptr;
    IC_74S00_U81* nand81_ic = nullptr;
    IC_74S138* ram_range_ic = nullptr;
    IC_74S138* ras_dec = nullptr;
    IC_74S08* ras_gate_ic = nullptr;
    IC_74S138* cas_dec = nullptr;
    IC_74S158* mux_lo_ic = nullptr;
    IC_74S158* mux_hi_ic = nullptr;
    IC_74S04* inv_ic = nullptr;
    IC_8237A* dma_ic = nullptr;
    IC_DRAM_256K dram;

    void wire(const std::string& bios_path) {
        // Build all_traces
        all_traces = {
            &vcc, &clk, &reset, &ready, &nmi, &intr, &test_pin,
            &cpu_lock, &rqgt0, &qs0, &qs1, &s0, &s1, &s2,
            &ale, &den, &dtr, &memr, &memw, &ior_sig, &iow_sig, &inta_sig,
            &dma_cs, &intr_cs, &pit_cs, &ppi_cs, &aen_bar,
            &osc, &pclk, &res,
            &holda, &hrq, &adstb, &dma_aen_out,
            &drq0, &drq1, &drq2, &drq3,
            &dack0_brd, &dack1, &dack2, &dack3, &eop,
        };
        for (int i = 0; i < 8; ++i)  all_traces.push_back(&ad[i]);
        for (int i = 0; i < 12; ++i) all_traces.push_back(&a_upper[i]);
        for (int i = 0; i < 8; ++i)  all_traces.push_back(d_arr[i]);
        for (int i = 0; i < 8; ++i)  all_traces.push_back(irq_arr[i]);
        for (int i = 0; i < 20; ++i) all_traces.push_back(&xa[i]);
        for (int i = 0; i < 8; ++i)  all_traces.push_back(dram_ma_arr[i]);
        for (int i = 0; i < 8; ++i)  all_traces.push_back(md_arr[i]);
        all_traces.push_back(&mdp);
        all_traces.push_back(&ras);
        all_traces.push_back(&cas);
        for (int i = 0; i < 4; ++i)  all_traces.push_back(ras_arr[i]);
        for (int i = 0; i < 4; ++i)  all_traces.push_back(dram_cas_arr[i]);
        all_traces.push_back(&dram_we);
        all_traces.push_back(&u83_mid);
        all_traces.push_back(&addr_sel);
        all_traces.push_back(&ram_addr_sel);
        all_traces.push_back(&refrsh_gate);
        all_traces.push_back(&bank_sel_y4); all_traces.push_back(&bank_sel_y5);
        all_traces.push_back(&bank_sel_y6); all_traces.push_back(&bank_sel_y7);
        all_traces.push_back(&rom_addr_sel);
        all_traces.push_back(&cs7);

        // U11: 8284A Clock Generator
        clk_socket.wire(2, pclk);    // PCLK output
        clk_socket.wire(5, ready);   // READY output
        clk_socket.wire(8, clk);     // CLK output
        clk_socket.wire(9, gnd);     // GND
        clk_socket.wire(10, reset);  // RESET output
        clk_socket.wire(11, res);    // RES input (PWR_GOOD)
        clk_socket.wire(12, osc);    // OSC output
        clk_socket.wire(18, vcc);    // VCC
        clk_gen = clk_socket.emplace<IC_8284A>();
        clk_gen->psu_wire(vcc.pin(), gnd.pin(), res.pin(), nmi.pin(),
                          s0.pin(), s1.pin(), s2.pin(), aen_bar.pin());

        // U3: 8088 CPU
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
        cpu = cpu_socket.emplace<IC_8088>(0x0100, 0x0100);

        // U6: 8288 Bus Controller
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
        bc = bc_socket.emplace<IC_8288>();

        // U8: 74S245 Data Bus Transceiver
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
        xcvr = xcvr_socket.emplace<IC_74S245>();

        // U12: 74S245 Memory Data Bus Transceiver
        // Bridges D0-D7 (system data bus) to MD0-MD7 (DRAM data bus).
        // Real 74S245: pin 1=DIR, pin 19=~OE.  Our IC_74S245: pin 1=~G, pin 19=DIR.
        // BRD: pin 1=~XMEMR (DIR), pin 19=~RAM_ADDR_SEL (~OE).
        // Wired to match IC_74S245's swapped convention: pin 1=~G=~RAM_ADDR_SEL, pin 19=DIR=~XMEMR.
        // DIR(~XMEMR) Low=B->A (MD->D, reads), High=A->B (D->MD, writes).
        // ~G(~RAM_ADDR_SEL) Low=enabled when address is in RAM range.
        mem_xcvr_socket.wire(1, ram_addr_sel);   // ~G = ~RAM_ADDR_SEL (~OE on real chip)
        mem_xcvr_socket.wire(2, d0);             // A1 = D0
        mem_xcvr_socket.wire(3, d1);             // A2 = D1
        mem_xcvr_socket.wire(4, d2);             // A3 = D2
        mem_xcvr_socket.wire(5, d3);             // A4 = D3
        mem_xcvr_socket.wire(6, d4);             // A5 = D4
        mem_xcvr_socket.wire(7, d5);             // A6 = D5
        mem_xcvr_socket.wire(8, d6);             // A7 = D6
        mem_xcvr_socket.wire(9, d7);             // A8 = D7
        mem_xcvr_socket.wire(10, gnd);
        mem_xcvr_socket.wire(11, md7);           // B8 = MD7
        mem_xcvr_socket.wire(12, md6);           // B7 = MD6
        mem_xcvr_socket.wire(13, md5);           // B6 = MD5
        mem_xcvr_socket.wire(14, md4);           // B5 = MD4
        mem_xcvr_socket.wire(15, md3);           // B4 = MD3
        mem_xcvr_socket.wire(16, md2);           // B3 = MD2
        mem_xcvr_socket.wire(17, md1);           // B2 = MD1
        mem_xcvr_socket.wire(18, md0);           // B1 = MD0
        mem_xcvr_socket.wire(19, memr);          // DIR = ~XMEMR (~MEMR, since U14 not implemented)
        mem_xcvr_socket.wire(20, vcc);
        mem_xcvr = mem_xcvr_socket.emplace<IC_74S245>();

        // U10: 74S373 Address Latch (low byte: AD0-AD7 -> XA0-XA7)
        // BRD: pin 1 (~OE) = AEN_BRD.  During normal CPU ops AEN_BRD is Low
        // (outputs enabled).  During DMA, AEN_BRD goes High to tri-state
        // latch outputs so the DMA controller can drive XA.
        // Without full DMA handshake, tie ~OE to GND (always enabled).
        latch_lo.wire(1, gnd);       // ~OE = GND (real board: AEN_BRD)
        latch_lo.wire(11, ale);      // LE = ALE
        latch_lo.wire(10, gnd);
        latch_lo.wire(20, vcc);
        // D pins: 3,4,7,8,13,14,17,18  Q pins: 2,5,6,9,12,15,16,19
        latch_lo.wire(3, ad[0]); latch_lo.wire(2, xa[0]);
        latch_lo.wire(4, ad[1]); latch_lo.wire(5, xa[1]);
        latch_lo.wire(7, ad[2]); latch_lo.wire(6, xa[2]);
        latch_lo.wire(8, ad[3]); latch_lo.wire(9, xa[3]);
        latch_lo.wire(13, ad[4]); latch_lo.wire(12, xa[4]);
        latch_lo.wire(14, ad[5]); latch_lo.wire(15, xa[5]);
        latch_lo.wire(17, ad[6]); latch_lo.wire(16, xa[6]);
        latch_lo.wire(18, ad[7]); latch_lo.wire(19, xa[7]);
        latch_lo_ic = latch_lo.emplace<IC_74S373>();

        // U9: 74S373 Address Latch (mid byte: A0-A7 -> XA8-XA15)
        latch_mid.wire(1, gnd);      // ~OE = GND (real board: AEN_BRD)
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
        latch_mid_ic = latch_mid.emplace<IC_74S373>();

        // U7: 74S373 Address Latch (high nibble: A8-A11 -> XA16-XA19)
        latch_hi.wire(1, gnd);       // ~OE = GND (real board: AEN_BRD)
        latch_hi.wire(11, ale);      // LE = ALE
        latch_hi.wire(10, gnd);
        latch_hi.wire(20, vcc);
        latch_hi.wire(3, a_upper[8]);  latch_hi.wire(2, xa[16]);
        latch_hi.wire(4, a_upper[9]);  latch_hi.wire(5, xa[17]);
        latch_hi.wire(7, a_upper[10]); latch_hi.wire(6, xa[18]);
        latch_hi.wire(8, a_upper[11]); latch_hi.wire(9, xa[19]);
        // D4-D7 unused on U7 -- only 4 address bits (A16-A19)
        latch_hi_ic = latch_hi.emplace<IC_74S373>();

        // U66: 74S138 I/O Address Decoder
        // Decodes XA5-XA7 when XA8=0, XA9=0, ~AEN=High (no DMA).
        // ~Y0=~DMA_CS (0x00), ~Y1=~INTR_CS (0x20), ~Y2=~PIT_CS (0x40), ~Y3=~PPI_CS (0x60)
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
        io_dec = io_decode.emplace<IC_74S138>();

        // U2: 8259A PIC
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
        pic = pic_socket.emplace<IC_8259A>();

        // U64: 74S20 Dual 4-Input NAND (ROM address decode)
        // Gate 1: NAND(A19, A18, A17, A16) -> ~ROM_ADDR_SEL
        // Active low when all four high bits are set (address >= F0000).
        nand_socket.wire(1, xa[19]);            // A1 = XA19
        nand_socket.wire(2, xa[18]);            // B1 = XA18
        nand_socket.wire(4, xa[17]);            // C1 = XA17
        nand_socket.wire(5, xa[16]);            // D1 = XA16
        nand_socket.wire(6, rom_addr_sel);      // Y1 = ~ROM_ADDR_SEL
        nand_socket.wire(7, gnd);
        nand_socket.wire(14, vcc);
        nand_ic = nand_socket.emplace<IC_74S20>();

        // U46: 74S138 ROM Chip Select Decoder
        // Decodes A15:A13 into ~CS2-~CS7 when ~ROM_ADDR_SEL=Low and ~MEMR=Low.
        // G1 = VCC (always enabled -- on real board this is ~RESET_DRV,
        //           but 8288 doesn't issue ~MEMR during reset, so safe).
        rom_decode.wire(1, xa[13]);             // A = XA13
        rom_decode.wire(2, xa[14]);             // B = XA14
        rom_decode.wire(3, xa[15]);             // C = XA15
        rom_decode.wire(4, memr);               // ~G2A = ~MEMR (active during memory read)
        rom_decode.wire(5, rom_addr_sel);       // ~G2B = ~ROM_ADDR_SEL
        rom_decode.wire(6, vcc);                // G1 = VCC (see note above)
        rom_decode.wire(7, cs7);                // ~Y7 = ~CS7 -> U33 (FE000-FFFFF)
        // ~Y0-~Y6 unconnected (no other ROM chips installed in test bench)
        rom_decode.wire(8, gnd);
        rom_decode.wire(16, vcc);
        rom_dec = rom_decode.emplace<IC_74S138>();

        // U33: 8K x 8 BIOS ROM (FE000-FFFFF)
        // Address pins wired to XA0-XA12, data pins to system data bus (D0-D7).
        // On the real board, ROM data pins connect to XD0-XD7 which reach D0-D7
        // through U13/U14 system bus transceivers. We connect directly to D0-D7
        // since U13/U14 are not modeled yet (U12 is DRAM-only).
        rom_socket.wire(1, xa[7]);              // A7
        rom_socket.wire(2, xa[6]);              // A6
        rom_socket.wire(3, xa[5]);              // A5
        rom_socket.wire(4, xa[4]);              // A4
        rom_socket.wire(5, xa[3]);              // A3
        rom_socket.wire(6, xa[2]);              // A2
        rom_socket.wire(7, xa[1]);              // A1
        rom_socket.wire(8, xa[0]);              // A0
        rom_socket.wire(9, d0);                 // D0
        rom_socket.wire(10, d1);                // D1
        rom_socket.wire(11, d2);                // D2
        rom_socket.wire(12, gnd);               // GND
        rom_socket.wire(13, d3);                // D3
        rom_socket.wire(14, d4);                // D4
        rom_socket.wire(15, d5);                // D5
        rom_socket.wire(16, d6);                // D6
        rom_socket.wire(17, d7);                // D7
        rom_socket.wire(18, xa[11]);            // A11
        rom_socket.wire(19, xa[10]);            // A10
        rom_socket.wire(20, cs7);               // ~CS
        rom_socket.wire(21, xa[12]);            // A12
        rom_socket.wire(22, xa[9]);             // A9
        rom_socket.wire(23, xa[8]);             // A8
        rom_socket.wire(24, vcc);               // VCC
        rom_ic = rom_socket.emplace<IC_ROM_8K>("BIOS_U33", bios_path);

        // --- DRAM: 4 banks x 9 chips = 36 sockets (IC_DRAM_256K) ---
        auto make_dram_bank = [](int start_u) -> std::vector<Socket> {
            std::vector<Socket> bank;
            bank.reserve(9);
            for (int i = 0; i < 9; ++i)
                bank.emplace_back("U" + std::to_string(start_u + i), "RAM_64K_X_1", 16);
            return bank;
        };
        ram_bank0 = make_dram_bank(37);  // U37-U45
        ram_bank1 = make_dram_bank(53);  // U53-U61
        ram_bank2 = make_dram_bank(69);  // U69-U77
        ram_bank3 = make_dram_bank(85);  // U85-U93

        // Wire each DRAM bank: shared MA/WE, per-bank RAS + CAS, per-chip data.
        auto wire_dram_bank = [&](std::vector<Socket>& bank, int bank_idx) {
            for (int chip = 0; chip < 9; ++chip) {
                auto& s = bank[chip];
                // 4164 address pins -> MA bus
                s.wire(5, dram_ma0);   // A0
                s.wire(7, dram_ma1);   // A1
                s.wire(6, dram_ma2);   // A2
                s.wire(12, dram_ma3);  // A3
                s.wire(11, dram_ma4);  // A4
                s.wire(10, dram_ma5);  // A5
                s.wire(13, dram_ma6);  // A6
                s.wire(9, dram_ma7);   // A7
                // Control
                s.wire(3, dram_we);    // ~WE
                s.wire(4, *ras_arr[bank_idx]);   // ~RAS (per-bank, from U49)
                s.wire(15, *dram_cas_arr[bank_idx]);  // ~CAS (per-bank, from U47)
                s.wire(8, vcc);        // VCC
                s.wire(16, gnd);       // GND
                // Data: chip 0 = parity, chips 1-8 = MD0-MD7
                if (chip == 0) {
                    s.wire(2, mdp);    // DIN (parity)
                    s.wire(14, mdp);   // DOUT (parity)
                } else {
                    s.wire(2, *md_arr[chip - 1]);   // DIN
                    s.wire(14, *md_arr[chip - 1]);  // DOUT
                }
            }
        };
        wire_dram_bank(ram_bank0, 0);
        wire_dram_bank(ram_bank1, 1);
        wire_dram_bank(ram_bank2, 2);
        wire_dram_bank(ram_bank3, 3);

        dram.install(ram_bank0, ram_bank1, ram_bank2, ram_bank3);

        // U62: 74S158 DRAM Address MUX (low nibble: MA0-MA3)
        // SELECT=ADDR_SEL, ~STROBE=GND (always enabled)
        // Low: MA[0:3] = ~{A0,A1,A2,A3}  High: MA[0:3] = ~{A8,A9,A10,A11}
        mux_lo.wire(1, addr_sel);          // SELECT
        mux_lo.wire(2, xa[0]);             // I0a = A0
        mux_lo.wire(3, xa[8]);             // I1a = A8
        mux_lo.wire(4, dram_ma0);          // Ya = MA0
        mux_lo.wire(5, xa[1]);             // I0b = A1
        mux_lo.wire(6, xa[9]);             // I1b = A9
        mux_lo.wire(7, dram_ma1);          // Yb = MA1
        mux_lo.wire(8, gnd);               // GND
        mux_lo.wire(9, dram_ma2);          // Yc = MA2
        mux_lo.wire(10, xa[10]);           // I1c = A10
        mux_lo.wire(11, xa[2]);            // I0c = A2
        mux_lo.wire(12, dram_ma3);         // Yd = MA3
        mux_lo.wire(13, xa[11]);           // I1d = A11
        mux_lo.wire(14, xa[3]);            // I0d = A3
        mux_lo.wire(15, gnd);              // ~STROBE = GND (always enabled)
        mux_lo.wire(16, vcc);              // VCC
        mux_lo_ic = mux_lo.emplace<IC_74S158>();

        // U79: 74S158 DRAM Address MUX (high nibble: MA4-MA7)
        mux_hi.wire(1, addr_sel);          // SELECT
        mux_hi.wire(2, xa[4]);             // I0a = A4
        mux_hi.wire(3, xa[12]);            // I1a = A12
        mux_hi.wire(4, dram_ma4);          // Ya = MA4
        mux_hi.wire(5, xa[5]);             // I0b = A5
        mux_hi.wire(6, xa[13]);            // I1b = A13
        mux_hi.wire(7, dram_ma5);          // Yb = MA5
        mux_hi.wire(8, gnd);               // GND
        mux_hi.wire(9, dram_ma6);          // Yc = MA6
        mux_hi.wire(10, xa[14]);           // I1c = A14
        mux_hi.wire(11, xa[6]);            // I0c = A6
        mux_hi.wire(12, dram_ma7);         // Yd = MA7
        mux_hi.wire(13, xa[15]);           // I1d = A15
        mux_hi.wire(14, xa[7]);            // I0d = A7
        mux_hi.wire(15, gnd);              // ~STROBE = GND (always enabled)
        mux_hi.wire(16, vcc);              // VCC
        mux_hi_ic = mux_hi.emplace<IC_74S158>();

        // U81: 74S00 NAND -- RAS/CAS generation + refresh gating.
        // Gate 1: DACK0 NAND RAS -> ~REFRSH_GATE
        // Gate 2: ~MEMR NAND ~MEMW -> RAS (fires when either memory cmd active)
        // Gate 3: N-000258 NAND N-000253 -> ~CAS (TD1 delayed RAS inputs)
        // Gate 4: unconnected (parity/channel check, N.P. signals)
        nand81_socket.wire(1, gnd);            // A1 = DACK0 (Low = no DMA)
        nand81_socket.wire(2, ras);            // B1 = RAS
        nand81_socket.wire(3, refrsh_gate);    // Y1 = ~REFRSH_GATE
        nand81_socket.wire(4, memr);           // A2 = ~MEMR
        nand81_socket.wire(5, memw);           // B2 = ~MEMW
        nand81_socket.wire(6, ras);            // Y2 = RAS
        nand81_socket.wire(7, gnd);
        nand81_socket.wire(8, cas);            // Y3 = ~CAS
        // Pins 9,10 not wired -- gate 3 uses virtual TD1 (delayed RAS) internally.
        nand81_socket.wire(14, vcc);
        nand81_ic = nand81_socket.emplace<IC_74S00_U81>();
        nand81_ic->connect_addr_sel(addr_sel);

        // U48: 74S138 RAM Address Range Select
        // A=GND, B=GND, C=A18, ~G2A=GND, ~G2B=A19, G1=VCC (no DMA).
        // ~Y0 = ~RAM_ADDR_SEL: active for addresses 0x00000-0x3FFFF.
        ram_range.wire(1, gnd);                // A = 0
        ram_range.wire(2, gnd);                // B = 0
        ram_range.wire(3, xa[18]);             // C = A18
        ram_range.wire(4, gnd);                // ~G2A = GND (always enabled)
        ram_range.wire(5, xa[19]);             // ~G2B = A19 (Low for < 512K)
        ram_range.wire(6, vcc);                // G1 = VCC (~DACK_0_BRD, no DMA)
        ram_range.wire(8, gnd);
        ram_range.wire(15, ram_addr_sel);      // ~Y0 = ~RAM_ADDR_SEL
        ram_range.wire(16, vcc);
        ram_range_ic = ram_range.emplace<IC_74S138>();

        // U65: 74S138 Per-Bank RAS Decoder
        // Decodes A16/A17 into bank selects, enabled by RAS + ~RAM_ADDR_SEL.
        // C=VCC so select range is 4-7 (outputs ~Y4-~Y7).
        ras_decode.wire(1, xa[16]);            // A = A16
        ras_decode.wire(2, xa[17]);            // B = A17
        ras_decode.wire(3, vcc);               // C = VCC (select 4-7)
        ras_decode.wire(4, gnd);               // ~G2A = DACK0 (Low = no DMA)
        ras_decode.wire(5, ram_addr_sel);      // ~G2B = ~RAM_ADDR_SEL
        ras_decode.wire(6, ras);               // G1 = RAS (active high)
        ras_decode.wire(7, bank_sel_y7);       // ~Y7 = N-000252 (bank 3)
        ras_decode.wire(8, gnd);
        ras_decode.wire(9, bank_sel_y6);       // ~Y6 = N-000255 (bank 2)
        ras_decode.wire(10, bank_sel_y5);      // ~Y5 = N-000251 (bank 1)
        ras_decode.wire(11, bank_sel_y4);      // ~Y4 = N-000256 (bank 0)
        ras_decode.wire(16, vcc);
        ras_dec = ras_decode.emplace<IC_74S138>();

        // U49: 74S08 Per-Bank RAS Gate
        // ANDs bank selects from U65 with ~REFRSH_GATE from U81.
        // Gate 3: N-000256 AND ~REFRSH_GATE -> ~RAS0
        // Gate 4: N-000251 AND ~REFRSH_GATE -> ~RAS1
        // Gate 1: N-000255 AND ~REFRSH_GATE -> ~RAS2
        // Gate 2: N-000252 AND ~REFRSH_GATE -> ~RAS3
        ras_gate.wire(1, bank_sel_y6);         // A1 = N-000255
        ras_gate.wire(2, refrsh_gate);         // B1 = ~REFRSH_GATE
        ras_gate.wire(3, ras2);                // Y1 = ~RAS2
        ras_gate.wire(4, bank_sel_y7);         // A2 = N-000252
        ras_gate.wire(5, refrsh_gate);         // B2 = ~REFRSH_GATE
        ras_gate.wire(6, ras3);                // Y2 = ~RAS3
        ras_gate.wire(7, gnd);
        ras_gate.wire(8, ras0);                // Y3 = ~RAS0
        ras_gate.wire(9, bank_sel_y4);         // A3 = N-000256
        ras_gate.wire(10, refrsh_gate);        // B3 = ~REFRSH_GATE
        ras_gate.wire(11, ras1);               // Y4 = ~RAS1
        ras_gate.wire(12, bank_sel_y5);        // A4 = N-000251
        ras_gate.wire(13, refrsh_gate);        // B4 = ~REFRSH_GATE
        ras_gate.wire(14, vcc);
        ras_gate_ic = ras_gate.emplace<IC_74S08>();

        // U47: 74S138 Per-Bank CAS Decoder
        // Decodes A16/A17 into per-bank ~CAS0-3, enabled by ~CAS + ~RAM_ADDR_SEL.
        cas_decode.wire(1, xa[16]);            // A = A16
        cas_decode.wire(2, xa[17]);            // B = A17
        cas_decode.wire(3, gnd);               // C = GND (select 0-3)
        cas_decode.wire(4, ram_addr_sel);      // ~G2A = ~RAM_ADDR_SEL
        cas_decode.wire(5, cas);               // ~G2B = ~CAS
        cas_decode.wire(6, vcc);               // G1 = VCC (~DACK_0_BRD, no DMA)
        cas_decode.wire(8, gnd);
        cas_decode.wire(12, dram_cas3);        // ~Y3 = ~CAS3
        cas_decode.wire(13, dram_cas2);        // ~Y2 = ~CAS2
        cas_decode.wire(14, dram_cas1);        // ~Y1 = ~CAS1
        cas_decode.wire(15, dram_cas0);        // ~Y0 = ~CAS0
        cas_decode.wire(16, vcc);
        cas_dec = cas_decode.emplace<IC_74S138>();

        // U83: 74S04 Hex Inverter (~WE buffer)
        // Gates 1+2 double-invert ~MEMW to buffer it for DRAM ~WE fan-out.
        // ~XMEMW -> pin1 -> pin2 (inverted) -> pin3 -> pin4 (restored) -> ~WE
        inv_socket.wire(1, memw);          // A1 = ~MEMW
        inv_socket.wire(2, u83_mid);       // Y1 = inverted ~MEMW
        inv_socket.wire(3, u83_mid);       // A2 = inverted ~MEMW
        inv_socket.wire(4, dram_we);       // Y2 = ~WE (double-inverted = buffered ~MEMW)
        inv_socket.wire(7, gnd);
        inv_socket.wire(14, vcc);
        inv_ic = inv_socket.emplace<IC_74S04>();

        // U35: 8237A DMA Controller
        // Wired per BRD. Series termination resistors aliased.
        // HOLDA=GND prevents DMA transfers (no bus handshake logic yet).
        // The 8237A responds to I/O ports 0x00-0x0F via ~DMA_CS from U66.
        dma_socket.wire(1, ior_sig);       // ~XIOR (aliased to ~IOR)
        dma_socket.wire(2, iow_sig);       // ~XIOW (aliased to ~IOW)
        dma_socket.wire(3, memr);          // ~XMEMR (DMA drives during transfer)
        dma_socket.wire(4, memw);          // ~XMEMW (DMA drives during transfer)
        dma_socket.wire(5, vcc);           // VCC
        dma_socket.wire(6, vcc);           // RDY_TO_DMA = always ready
        dma_socket.wire(7, gnd);           // HOLDA = never grant bus
        dma_socket.wire(8, adstb);         // ADSTB (N-000280 aliased)
        dma_socket.wire(9, dma_aen_out);   // AEN output (unconnected on real 5150)
        dma_socket.wire(10, hrq);          // HRQ (N-000286 aliased)
        dma_socket.wire(11, dma_cs);       // ~DMA_CS (from U66)
        dma_socket.wire(12, clk);          // DCLK (aliased to CLK)
        dma_socket.wire(13, reset);        // RESET
        dma_socket.wire(14, dack2);        // ~DACK2
        dma_socket.wire(15, dack3);        // ~DACK3
        dma_socket.wire(16, drq3);         // DRQ3
        dma_socket.wire(17, drq2);         // DRQ2
        dma_socket.wire(18, drq1);         // DRQ1
        dma_socket.wire(19, drq0);         // DRQ0
        dma_socket.wire(20, gnd);          // GND
        dma_socket.wire(21, d7);           // XD7 (aliased to D7)
        dma_socket.wire(22, d6);
        dma_socket.wire(23, d5);
        dma_socket.wire(24, dack1);        // ~DACK1
        dma_socket.wire(25, dack0_brd);    // ~DACK_0_BRD
        dma_socket.wire(26, d4);
        dma_socket.wire(27, d3);
        dma_socket.wire(28, d2);
        dma_socket.wire(29, d1);
        dma_socket.wire(30, d0);           // XD0 (aliased to D0)
        dma_socket.wire(31, vcc);          // VCC
        dma_socket.wire(32, xa[0]);        // XA0
        dma_socket.wire(33, xa[1]);        // XA1
        dma_socket.wire(34, xa[2]);        // XA2
        dma_socket.wire(35, xa[3]);        // XA3
        dma_socket.wire(36, eop);          // ~EOP (N-000281 aliased)
        dma_socket.wire(37, xa[4]);        // A4 (N-000285 aliased to XA4)
        dma_socket.wire(38, xa[5]);        // A5 (N-000282 aliased to XA5)
        dma_socket.wire(39, xa[6]);        // A6 (N-000284 aliased to XA6)
        dma_socket.wire(40, xa[7]);        // A7 (N-000283 aliased to XA7)
        dma_ic = dma_socket.emplace<IC_8237A>();

        // --- ISA Slots (J1-J5) ---
        // Wired directly to motherboard signals. Buffer ICs (U14-U17) omitted
        // because series termination resistors are aliased and no DMA is present.
        // All 5 slots share identical wiring (parallel bus).
        for (auto& slot : isa_slots) {
            // Data bus: SD0-SD7 = D0-D7 (pins 2-9 = SD7..SD0)
            slot.wire_pin(2, &d7);  slot.wire_pin(3, &d6);
            slot.wire_pin(4, &d5);  slot.wire_pin(5, &d4);
            slot.wire_pin(6, &d3);  slot.wire_pin(7, &d2);
            slot.wire_pin(8, &d1);  slot.wire_pin(9, &d0);

            // Address bus: SA0-SA19 = XA0-XA19 (pins 31..12 = SA0..SA19)
            for (int a = 0; a < 20; ++a)
                slot.wire_pin(31 - a, &xa[a]);

            // Control signals
            slot.wire_pin(11, &gnd);       // AEN (A11) = Low (no DMA)
            slot.wire_pin(42, &memw);      // ~MEMW (B11)
            slot.wire_pin(43, &memr);      // ~MEMR (B12)
            slot.wire_pin(44, &iow_sig);   // ~IOW (B13)
            slot.wire_pin(45, &ior_sig);   // ~IOR (B14)

            // Clocks
            slot.wire_pin(51, &clk);       // CLK (B20)
            slot.wire_pin(61, &osc);       // OSC (B30)

            // Reset
            slot.wire_pin(33, &reset);     // RESET DRV (B2)

            // AEN_BRD on B28 (IBM 5150 uses this pin for AEN_BRD, not ALE)
            slot.wire_pin(59, &gnd);       // AEN_BRD = Low (no DMA)

            // IRQ lines
            slot.wire_pin(35, &irq2);      // IRQ2 (B4)
            slot.wire_pin(52, &irq7);      // IRQ7 (B21)
            slot.wire_pin(53, &irq6);      // IRQ6 (B22)
            slot.wire_pin(54, &irq5);      // IRQ5 (B23)
            slot.wire_pin(55, &irq4);      // IRQ4 (B24)
            slot.wire_pin(56, &irq3);      // IRQ3 (B25)

            // DMA (active-low DACKs from 8237A, DRQs are inputs from cards)
            slot.wire_pin(46, &dack3);     // ~DACK3 (B15)
            slot.wire_pin(47, &drq3);      // DRQ3 (B16)
            slot.wire_pin(48, &dack1);     // ~DACK1 (B17)
            slot.wire_pin(49, &drq1);      // DRQ1 (B18)
            slot.wire_pin(50, &dack0_brd); // ~DACK0 (B19)
            slot.wire_pin(57, &dack2);     // ~DACK2 (B26)
            slot.wire_pin(37, &drq2);      // DRQ2 (B6)
            slot.wire_pin(58, &eop);       // T/C (B27)

            // Power rails
            slot.wire_pin(32, &gnd);       // GND (B1)
            slot.wire_pin(41, &gnd);       // GND (B10)
            slot.wire_pin(62, &gnd);       // GND (B31)
            slot.wire_pin(34, &vcc);       // +5V (B3)
            slot.wire_pin(60, &vcc);       // +5V (B29)
            slot.wire_pin(36, &gnd);       // -5V (B5) stub
            slot.wire_pin(38, &gnd);       // -12V (B7) stub
            slot.wire_pin(40, &gnd);       // +12V (B9) stub
        }
    }

    void register_all(Scheduler& scheduler) {
        scheduler.register_callback(xcvr);
        scheduler.register_callback(mem_xcvr);
        scheduler.register_callback(latch_lo_ic);
        scheduler.register_callback(latch_mid_ic);
        scheduler.register_callback(latch_hi_ic);
        scheduler.register_callback(io_dec);
        scheduler.register_callback(nand_ic);
        scheduler.register_callback(rom_dec);
        scheduler.register_callback(rom_ic);
        scheduler.register_callback(mux_lo_ic);
        scheduler.register_callback(mux_hi_ic);
        scheduler.register_callback(nand81_ic);
        scheduler.register_callback(ram_range_ic);
        scheduler.register_callback(ras_dec);
        scheduler.register_callback(ras_gate_ic);
        scheduler.register_callback(cas_dec);
        scheduler.register_callback(inv_ic);
        scheduler.register_callback(dma_ic);
        scheduler.register_callback(&dram);
        scheduler.register_callback(bc);
        scheduler.register_callback(pic);
        scheduler.register_fiber(cpu);
        scheduler.register_visual(clk_gen);
        clk_gen->set_scheduler(&scheduler);
        cpu->set_scheduler(&scheduler);
    }
};
