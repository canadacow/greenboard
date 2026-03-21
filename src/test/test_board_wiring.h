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
#include "ic/ic_74s175.h"
#include "ic/ic_74s74.h"
#include "ic/ic_74ls30.h"
#include "ic/ic_74ls670.h"
#include "ic/ic_8237a.h"
#include "ic/ic_74s10.h"
#include "ic/ic_74ls02.h"
#include "ic/ic_74ls32.h"
#include "ic/ic_8253.h"
#include "board/isa_slot.h"
#include <string>
#include <vector>

using namespace bench;

struct TestBoard {
    bool dma_enabled = true;

    // --- Signals (copper traces) ---
    // Pre-allocate contiguous blocks for IC outputs.
    int u97_block_ = SignalPool::allocate_block(4);  // U97 74S08: Y1=NMI, Y2=nc, Y3=RDY_TO_DMA, Y4=N-000246
    int ad_block_  = SignalPool::allocate_block(8);  // AD0-AD7: U10 74S373 D inputs
    int a_block_   = SignalPool::allocate_block(16); // A0-A11 + 4 dummy: U9/U7 74S373 D inputs
    int la_block_  = SignalPool::allocate_block(24); // LA0-LA19 + 4 dummy: 74S373 Q outputs (BRD "A0-A19")
    int xa_block_  = SignalPool::allocate_block(24); // XA0-XA19 + 4 dummy: buffered via U15/U16/U17 (BRD "XA0-XA19")
    int d_block_   = SignalPool::allocate_block(8);  // D0-D7: system data bus (U18 74S373 D inputs)

    Signal vcc{"+5V"}, gnd{"GND"}, clk{"CLK"}, reset{"RESET"};
    Signal ready{"READY"}, nmi{"NMI", u97_block_}, intr{"INTR"}, test_pin{"~TEST"};
    Signal cpu_lock{"~LOCK"}, rqgt0{"~RQ/GT0"};
    Signal qs0{"QS0"}, qs1{"QS1"}, s0{"~S0"}, s1{"~S1"}, s2{"~S2"};
    Bus ad{"AD", 8, ad_block_};
    Bus a_upper{"A", 12, a_block_};

    // 8288 bus controller output signals
    Signal ale{"ALE"}, den{"~DEN"}, dtr{"DT/~R"};
    Signal memr{"~MEMR"}, memw{"~MEMW"};
    Signal ior_sig{"~IOR"}, iow_sig{"~IOW"}, inta_sig{"~INTA"};

    // X-side signals: buffered through U13 (data) and U14 (control).
    // These reach PIT, PIC, DMA, ROMs, PPI, and other motherboard ICs.
    int xd_block_ = SignalPool::allocate_block(8);
    Signal xd0{"XD0", xd_block_},     xd1{"XD1", xd_block_ + 1};
    Signal xd2{"XD2", xd_block_ + 2}, xd3{"XD3", xd_block_ + 3};
    Signal xd4{"XD4", xd_block_ + 4}, xd5{"XD5", xd_block_ + 5};
    Signal xd6{"XD6", xd_block_ + 6}, xd7{"XD7", xd_block_ + 7};
    Signal* xd_arr[8] = {&xd0, &xd1, &xd2, &xd3, &xd4, &xd5, &xd6, &xd7};
    Signal xior{"~XIOR"}, xiow{"~XIOW"}, xmemr{"~XMEMR"}, xmemw{"~XMEMW"};
    Signal n_000290{"N-000290"};      // U27 gate 1 output -> U13 DIR
    // N-000304 is the ~XMEMW net through a series termination resistor.
    // Alias it to xmemw since we don't model the resistor.
    Signal& n_000304 = xmemw;

    // U66 I/O decode outputs
    Signal dma_cs{"~DMA_CS"}, intr_cs{"~INTR_CS"}, pit_cs{"~PIT_CS"}, ppi_cs{"~PPI_CS"};
    Signal aen_bar{"~AEN"};  // No DMA in test bench, always High

    // System data bus (B side of 74S245 transceiver)
    Signal d0{"D0", d_block_},     d1{"D1", d_block_ + 1};
    Signal d2{"D2", d_block_ + 2}, d3{"D3", d_block_ + 3};
    Signal d4{"D4", d_block_ + 4}, d5{"D5", d_block_ + 5};
    Signal d6{"D6", d_block_ + 6}, d7{"D7", d_block_ + 7};
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
    // U49 outputs: Y1=~RAS2, Y2=~RAS3, Y3=~RAS0, Y4=~RAS1 (contiguous block)
    int ras_block_ = SignalPool::allocate_block(4);
    Signal ras2{"~RAS2", ras_block_},     ras3{"~RAS3", ras_block_ + 1};
    Signal ras0{"~RAS0", ras_block_ + 2}, ras1{"~RAS1", ras_block_ + 3};
    Signal* ras_arr[4] = {&ras0, &ras1, &ras2, &ras3};
    Signal dram_cas0{"~CAS0"}, dram_cas1{"~CAS1"}, dram_cas2{"~CAS2"}, dram_cas3{"~CAS3"};
    Signal* dram_cas_arr[4] = {&dram_cas0, &dram_cas1, &dram_cas2, &dram_cas3};

    // DMA controller signals
    Signal holda{"HOLDA"};              // U67 FF1 Q -> 8237A HLDA
    Signal hrq{"HRQ"};                  // Hold request (from 8237A, N-000286)
    Signal adstb{"ADSTB"};              // Address strobe (for DMA page latch, N-000280)
    Signal dma_aen_out{"DMA_AEN"};      // AEN output from 8237A pin 9
    Signal drq0{"DRQ0"};               // DMA request 0 (U67 FF2 Q -> 8237A)
    Signal drq1{"DRQ1"}, drq2{"DRQ2"}, drq3{"DRQ3"};
    Signal dack0_brd{"~DACK_0_BRD"};   // ~DACK0 from 8237A
    Signal dack1{"~DACK1"}, dack2{"~DACK2"}, dack3{"~DACK3"};
    Signal eop{"~EOP"};                 // End of process / terminal count

    // DMA address intermediate nets (8237A A4-A7 outputs -> U17 Group 2 inputs)
    // Separate from la[4]-la[7] because U17 buffers these onto the A bus.
    Signal dma_a4{"N-000285"};          // 8237A pin 37 -> U17 pin 11
    Signal dma_a5{"N-000282"};          // 8237A pin 38 -> U17 pin 13
    Signal dma_a6{"N-000284"};          // 8237A pin 39 -> U17 pin 15
    Signal dma_a7{"N-000283"};          // 8237A pin 40 -> U17 pin 17

    // DMA bus grant handshake signals
    Signal aen_brd{"AEN_BRD"};          // U98 FF1 Q: Low=CPU, High=DMA owns bus
    Signal nclk88{"N-000247"};          // ~CLK88 from U99 inverter 4
    Signal hrq_dma_bar{"~HRQ_DMA"};    // U99 inv1: ~HRQ
    Signal n_000242{"N-000242"};        // U52 gate1 -> U5 + U67 ~CLR
    Signal n_000243{"N-000243"};        // U5 output (bus idle NAND)
    Signal n_000238{"N-000238"};        // U83 inv4 output -> U98 FF4 D
    Signal n_000231{"N-000231"};        // U98 FF4 Q -> U67 FF1 D
    Signal n_000230{"N-000230"};        // U67 FF1 ~Q -> ~PRE1 (feedback)
    Signal dclk{"DCLK"};               // U52 gate2: DMA clock
    Signal tc{"T/C"};                   // U99 inv2: inverted ~EOP
    Signal reset_drv_bar{"~RESET_DRV"}; // U51 inv1: ~RESET
    Signal n_000328{"N-000328"};        // PIT OUT1 -> U67 FF2 CLK (stub)
    // ~DMA_AEN: from U50 gate 1 = NOR(N-000246, N-000246) = ~N-000246.
    // Separate signal from ~AEN (U98 ~1Q) -- they track differently during transitions.
    Signal dma_aen_bar{"~DMA_AEN"};
    Signal dma_wait_bar{"~DMA_WAIT"};   // U98 ~2Q (pin 6)
    Signal rdy_wait_bar{"~RDY~/WAIT"};  // U82 -> U98 3D (pin 12)
    Signal io_ch_rdy{"I/O_CH_RDY"};    // ISA bus -> U82 ~PRE2 -> 8284A ~AEN1

    // Glue logic intermediate signals (U84, U97, U27, U101)
    Signal u97_y2_nc{"U97_Y2", u97_block_ + 1};          // U97 gate 2 unused
    Signal rdy_to_dma{"RDY_TO_DMA", u97_block_ + 2};     // U97 gate 3 output -> DMA pin 6
    Signal n_000244{"N-000244"};      // U98 3Q (pin 11) -> U97 gate 3 input
    Signal n_000245{"N-000245"};      // U98 2Q (pin 7) -> U97 gate 4 input
    Signal n_000246{"N-000246", u97_block_ + 3};          // U97 gate 4 output
    Signal n_000235{"N-000235"};      // U84 gate 3 output -> U97 gate 1 input
    Signal n_000225{"N-000225"};      // Parity check stub -> U97 gate 1 B input (PSU NMI)
    Signal n_000215{"N-000215"};      // U84 gate 1 output (inverted DT/~R)
    Signal n_000239{"N-000239"};      // U84 gate 2 output
    Signal n_000241{"N-000241"};      // U83 gate 5 output (inverted ~XMEMR)
    Signal n_000240{"N-000240"};      // U64 gate 2 output -> U82 CLK2
    Signal n_000237{"N-000237"};      // U82 FF2 ~Q -> U97 gate 3 A input
    Signal u101_y4{"N-000291"};       // U101 gate 4 output -> U5 pin 3
    Signal pg_reg_cs{"N-000259"};      // U66 ~Y4: I/O decode 0x80-0x9F
    Signal wrt_dma_pg{"~WRT_DMA_PG_REG"}; // U51 inv6 output -> U19 ~WE
    Signal n_000261{"N-000261"};              // U50 gate 4 -> U51 inv6 input
    Signal pit_rd{"PIT_~RD"};                 // U101 gate 2 output -> U34 pin 22
    Signal pit_wr{"PIT_~WR"};                 // U101 gate 1 output -> U34 pin 23
    Signal isa_aen{"ISA_AEN"};                // Buffered AEN for ISA bus (from U15)
    Signal isa_dack0{"ISA_~DACK0"};           // Buffered ~DACK0 for ISA bus (from U15)
    Signal isa_clk88{"N-000289"};             // Buffered CLK88 for ISA bus (from U15, stub)
    Signal n_000288{"N-000288"};      // U27 gate 2 output
    Signal n_000317{"N-000317"};      // U27 gate 3 output
    Signal n_000303{"N-000303"};      // U27 gate 4 output

    // 8284A signals
    Signal osc{"OSC"}, pclk{"PCLK"}, res{"RES"};

    // U26 PCLK divider signals
    Signal pit_clk{"PIT_CLK"};          // U26 FF2 Q = 1.193 MHz (PCLK / 2)
    Signal pclk_div2_fb{"PCLK_DIV2_FB"}; // U26 FF2 ~Q -> D feedback

    // Latch address bus (outputs from 74S373 latches -- BRD net names "A0-A19")
    // These feed U15/U16/U17 address buffers, which output XA0-XA19.
    Signal la[20] = {
        Signal("LA0",  la_block_),      Signal("LA1",  la_block_ + 1),
        Signal("LA2",  la_block_ + 2),  Signal("LA3",  la_block_ + 3),
        Signal("LA4",  la_block_ + 4),  Signal("LA5",  la_block_ + 5),
        Signal("LA6",  la_block_ + 6),  Signal("LA7",  la_block_ + 7),
        Signal("LA8",  la_block_ + 8),  Signal("LA9",  la_block_ + 9),
        Signal("LA10", la_block_ + 10), Signal("LA11", la_block_ + 11),
        Signal("LA12", la_block_ + 12), Signal("LA13", la_block_ + 13),
        Signal("LA14", la_block_ + 14), Signal("LA15", la_block_ + 15),
        Signal("LA16", la_block_ + 16), Signal("LA17", la_block_ + 17),
        Signal("LA18", la_block_ + 18), Signal("LA19", la_block_ + 19),
    };

    // Buffered address bus (outputs from U15/U16/U17 -- BRD net names "XA0-XA19")
    // All motherboard ICs and ISA slots connect here.
    Signal xa[20] = {
        Signal("XA0",  xa_block_),      Signal("XA1",  xa_block_ + 1),
        Signal("XA2",  xa_block_ + 2),  Signal("XA3",  xa_block_ + 3),
        Signal("XA4",  xa_block_ + 4),  Signal("XA5",  xa_block_ + 5),
        Signal("XA6",  xa_block_ + 6),  Signal("XA7",  xa_block_ + 7),
        Signal("XA8",  xa_block_ + 8),  Signal("XA9",  xa_block_ + 9),
        Signal("XA10", xa_block_ + 10), Signal("XA11", xa_block_ + 11),
        Signal("XA12", xa_block_ + 12), Signal("XA13", xa_block_ + 13),
        Signal("XA14", xa_block_ + 14), Signal("XA15", xa_block_ + 15),
        Signal("XA16", xa_block_ + 16), Signal("XA17", xa_block_ + 17),
        Signal("XA18", xa_block_ + 18), Signal("XA19", xa_block_ + 19),
    };

    // U7 only uses 4 of 8 D/Q pairs. Pad with dummies to keep PinBlock<8> contiguous.
    Signal u7_d_pad[4] = {
        Signal("U7_D4", a_block_ + 12), Signal("U7_D5", a_block_ + 13),
        Signal("U7_D6", a_block_ + 14), Signal("U7_D7", a_block_ + 15),
    };
    Signal u7_q_pad[4] = {
        Signal("U7_Q4", la_block_ + 20), Signal("U7_Q5", la_block_ + 21),
        Signal("U7_Q6", la_block_ + 22), Signal("U7_Q7", la_block_ + 23),
    };

    // ROM-specific signals
    Signal rom_addr_sel{"~ROM_ADDR_SEL"};
    Signal cs7{"~CS7"};

    // ISA bus buffer sockets
    Socket xcvr13_socket{"U13", "74S245", 20};   // D <-> XD data transceiver
    Socket xcvr14_socket{"U14", "74S245", 20};   // cmd strobes: ~IOR/~IOW/~MEMR/~MEMW <-> ~XIOR/~XIOW/~XMEMR/~XMEMW
    Socket buf15_socket{"U15", "74S244", 20};     // Address buffer (XA8-XA12, misc)
    Socket buf16_socket{"U16", "74S244", 20};     // Address buffer (XA0-XA7 -> A0-A7)
    Socket buf17_socket{"U17", "74S244", 20};     // Address buffer (XA0-XA3 + DMA A4-A7)

    // ISA bus slots
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
    Socket inv99_socket{"U99", "74S04", 14};   // Hex inverter (HRQ, CLK88, EOP)
    Socket nand52_socket{"U52", "74S00", 14};  // Quad NAND (HRQ gate, DCLK)
    Socket nand5_socket{"U5", "74LS30", 14};   // 8-input NAND (bus idle)
    Socket ff67_socket{"U67", "74S74", 14};    // Dual D FF (HOLDA, DRQ0 latch)
    Socket ff82_socket{"U82", "74S74", 14};    // Dual D FF (keyboard IRQ1, DMA wait state)
    Socket ff98_socket{"U98", "74S175", 16};   // Quad D FF (AEN_BRD)
    Socket inv51_socket{"U51", "74S04", 14};   // Hex inverter (~RESET_DRV)
    Socket dma_page_latch{"U18", "74S373", 20};  // DMA address latch (A8-A15)
    Socket dma_page_reg{"U19", "74LS670", 16};   // DMA page register (A16-A19)
    Socket nand84_socket{"U84", "74S10", 14};    // Triple 3-input NAND (DACK/AEN gating)
    Socket and97_socket{"U97", "74S08", 14};     // Quad AND (NMI, RDY_TO_DMA)
    Socket nor27_socket{"U27", "74LS02", 14};    // Quad NOR (ROM/RAM/IO select)
    Socket or101_socket{"U101", "74LS32", 14};   // Quad OR (PIT ~RD/~WR, ~DMA_CS gating)
    Socket nor50_socket{"U50", "74S02", 14};    // Quad NOR (~DMA_AEN, ~WRT_DMA_PG_REG path)
    Socket ff26_socket{"U26", "74S175", 16};       // Quad D FF (PCLK divider)
    Socket pit_socket{"U34", "8253", 24};           // PIT (timer)
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
    IC_74S138<0x1F>* io_dec = nullptr;      // U66: Y0-Y4
    IC_8259A* pic = nullptr;
    IC_74S20<0x03>* nand_ic = nullptr;
    IC_74S138<0x80>* rom_dec = nullptr;     // U46: Y7
    IC_ROM_8K* rom_ic = nullptr;
    IC_74S00_U81* nand81_ic = nullptr;
    IC_74S138<0x01>* ram_range_ic = nullptr; // U48: Y0
    IC_74S138<0xF0>* ras_dec = nullptr;     // U65: Y4-Y7
    IC_74S08<0xFF, true>* ras_gate_ic = nullptr;  // U49: all AND, shared B
    IC_74S138<0x0F>* cas_dec = nullptr;     // U47: Y0-Y3
    IC_74S158* mux_lo_ic = nullptr;
    IC_74S158* mux_hi_ic = nullptr;
    IC_74S04* inv_ic = nullptr;
    IC_8237A* dma_ic = nullptr;
    IC_74S04* inv99_ic = nullptr;     // U99
    IC_74S00* nand52_ic = nullptr;    // U52
    IC_74LS30* nand5_ic = nullptr;    // U5
    IC_74S74* ff67_ic = nullptr;      // U67
    IC_74S74* ff82_ic = nullptr;      // U82
    IC_74S175* ff98_ic = nullptr;     // U98
    IC_74S04* inv51_ic = nullptr;     // U51
    IC_74S373* dma_page_latch_ic = nullptr;  // U18
    IC_74LS670* dma_page_reg_ic = nullptr;   // U19
    IC_74S10* nand84_ic = nullptr;             // U84
    IC_74S08<0xE3>* and97_ic = nullptr;         // U97: g0=passA(NMI), g1=dead, g2=passB(RDY), g3=AND
    IC_74LS02* nor27_ic = nullptr;             // U27
    IC_74LS32* or101_ic = nullptr;             // U101
    IC_74LS02* nor50_ic = nullptr;              // U50
    IC_74S175* ff26_ic = nullptr;               // U26
    IC_8253* pit_ic = nullptr;                  // U34
    IC_74S245* xcvr13_ic = nullptr;     // U13: D <-> XD
    IC_74S245* xcvr14_ic = nullptr;     // U14: cmd buffer
    IC_74S244* buf15_ic = nullptr;      // U15: address buffer
    IC_74S244* buf16_ic = nullptr;      // U16: address buffer
    IC_74S244* buf17_ic = nullptr;      // U17: address buffer
    IC_DRAM_256K dram;

    void wire(const std::string& bios_path) {
        // Power rails never create dependency edges.
        vcc.set_power_rail();
        gnd.set_power_rail();

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
            &aen_brd, &nclk88, &hrq_dma_bar, &n_000242,
            &n_000243, &n_000238, &n_000231, &n_000230,
            &dclk, &tc, &reset_drv_bar, &n_000328,
            &u97_y2_nc, &rdy_to_dma, &n_000244, &n_000245, &n_000246,
            &n_000235, &n_000215, &n_000239, &n_000241, &n_000240, &u101_y4,
            &n_000288, &n_000317, &n_000303, &pg_reg_cs, &wrt_dma_pg,
            &pit_clk, &pclk_div2_fb,
            &xior, &xiow, &xmemr, &xmemw, &n_000290, &dma_aen_bar,
            &n_000261, &pit_rd, &pit_wr, &isa_aen, &isa_dack0, &isa_clk88,
        };
        for (int i = 0; i < 8; ++i)  all_traces.push_back(&ad[i]);
        for (int i = 0; i < 12; ++i) all_traces.push_back(&a_upper[i]);
        for (int i = 0; i < 8; ++i)  all_traces.push_back(d_arr[i]);
        for (int i = 0; i < 8; ++i)  all_traces.push_back(xd_arr[i]);
        for (int i = 0; i < 8; ++i)  all_traces.push_back(irq_arr[i]);
        for (int i = 0; i < 20; ++i) all_traces.push_back(&la[i]);
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
        clk_socket.wire(3, io_ch_rdy);  // ~AEN1 = I/O_CH_RDY (directly, bypassing U82 noise)
        clk_socket.wire(4, aen_bar);  // RDY1 = ~AEN (U98 ~1Q) -- Low when DMA active, High when CPU
        clk_socket.wire(5, ready);   // READY output
        clk_socket.wire(8, clk);     // CLK output
        clk_socket.wire(9, gnd);     // GND
        clk_socket.wire(10, reset);  // RESET output
        clk_socket.wire(11, res);    // RES input (PWR_GOOD)
        clk_socket.wire(12, osc);    // OSC output
        clk_socket.wire(18, vcc);    // VCC
        clk_gen = clk_socket.emplace<IC_8284A>();
        clk_gen->psu_wire(vcc.pin(), gnd.pin(), res.pin(), n_000225.pin(),
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
        bc_socket.wire(6, aen_bar);  // CEN = ~AEN (U98 ~1Q, Low=DMA active)
        bc_socket.wire(7, memr);
        bc_socket.wire(8, memw);
        bc_socket.wire(12, iow_sig);
        bc_socket.wire(13, ior_sig);
        bc_socket.wire(14, inta_sig);
        bc_socket.wire(15, rdy_wait_bar);  // ~AEN = ~RDY/WAIT (U82 FF2 Q, synced with 8284A ~AEN1)
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
        mem_xcvr_socket.wire(19, xmemr);         // DIR = ~XMEMR
        mem_xcvr_socket.wire(20, vcc);
        mem_xcvr = mem_xcvr_socket.emplace<IC_74S245>();
        
        // U13: 74S245 System Data Bus Transceiver (D0-D7 <-> XD0-XD7)
        // BRD: pin 1=N-000290 (DIR on real chip), pin 19=AEN_BRD (~OE on real chip).
        // Our IC_74S245 swaps: pin 1=~G, pin 19=DIR.
        // So wire: pin 1=~G=AEN_BRD, pin 19=DIR=N-000290.
        // DIR(N-000290) Low=B->A (XD->D, reads), High=A->B (D->XD, writes).
        xcvr13_socket.wire(1, aen_brd);          // ~G = AEN_BRD (~OE: Low=enabled during CPU)
        xcvr13_socket.wire(2, d0);               // A1 = D0
        xcvr13_socket.wire(3, d1);               // A2 = D1
        xcvr13_socket.wire(4, d2);               // A3 = D2
        xcvr13_socket.wire(5, d3);               // A4 = D3
        xcvr13_socket.wire(6, d4);               // A5 = D4
        xcvr13_socket.wire(7, d5);               // A6 = D5
        xcvr13_socket.wire(8, d6);               // A7 = D6
        xcvr13_socket.wire(9, d7);               // A8 = D7
        xcvr13_socket.wire(10, gnd);
        xcvr13_socket.wire(11, xd7);             // B8 = XD7
        xcvr13_socket.wire(12, xd6);             // B7 = XD6
        xcvr13_socket.wire(13, xd5);             // B6 = XD5
        xcvr13_socket.wire(14, xd4);             // B5 = XD4
        xcvr13_socket.wire(15, xd3);             // B4 = XD3
        xcvr13_socket.wire(16, xd2);             // B3 = XD2
        xcvr13_socket.wire(17, xd1);             // B2 = XD1
        xcvr13_socket.wire(18, xd0);             // B1 = XD0
        xcvr13_socket.wire(19, n_000290);        // DIR = N-000290 (U27 gate 1)
        xcvr13_socket.wire(20, vcc);
        xcvr13_ic = xcvr13_socket.emplace<IC_74S245>();

        // U14: 74S245 Command Strobe Transceiver
        // BRD: pin 1=~DMA_AEN (DIR on real chip), pin 19=GND (~OE on real chip).
        // Our IC_74S245 swaps: pin 1=~G, pin 19=DIR.
        // So wire: pin 1=~G=GND (always enabled), pin 19=DIR=~DMA_AEN.
        // DIR(~DMA_AEN) High=A->B (8288->X, CPU mode), Low=B->A (X->8288, DMA mode).
        // Only 4 of 8 channels used; unused pins left unwired.
        xcvr14_socket.wire(1, gnd);              // ~G = GND (always enabled)
        xcvr14_socket.wire(2, ior_sig);          // A1 = ~IOR (from 8288)
        xcvr14_socket.wire(3, iow_sig);          // A2 = ~IOW
        xcvr14_socket.wire(4, memr);             // A3 = ~MEMR
        xcvr14_socket.wire(5, memw);             // A4 = ~MEMW
        xcvr14_socket.wire(10, gnd);
        xcvr14_socket.wire(15, xmemw);           // B4 = ~XMEMW
        xcvr14_socket.wire(16, xmemr);           // B3 = ~XMEMR
        xcvr14_socket.wire(17, xiow);            // B2 = ~XIOW
        xcvr14_socket.wire(18, xior);            // B1 = ~XIOR
        xcvr14_socket.wire(19, dma_aen_bar);     // DIR = ~DMA_AEN
        xcvr14_socket.wire(20, vcc);
        xcvr14_ic = xcvr14_socket.emplace<IC_74S245>();
        // ~DMA_AEN feeds back through DMA chain (U14->~XMEMR->...->U50->~DMA_AEN->U14).
        // Break the DAG cycle by marking DIR as async -- it only changes between bus cycles.
        //xcvr14_ic->declare_async_input(xcvr14_socket.pin_signal(19)->pin());

        // Fold all four transceivers into the 8288 so transfers happen
        // synchronously with ~DEN/DT/~R assertion (no one-cycle DAG lag).
        bc->set_xcvr(xcvr, xcvr13_ic, mem_xcvr, xcvr14_ic);
        bc->set_addr_hi(la[18].pin(), la[19].pin());

        // U10: 74S373 Address Latch (low byte: AD0-AD7 -> XA0-XA7)
        // BRD: pin 1 (~OE) = AEN_BRD.  During normal CPU ops AEN_BRD is Low
        // (outputs enabled).  During DMA, AEN_BRD goes High to tri-state
        // latch outputs so the DMA controller can drive XA.
        // Without full DMA handshake, tie ~OE to GND (always enabled).
        latch_lo.wire(1, aen_brd);   // ~OE = AEN_BRD (Low=enabled, High=DMA tri-states)
        latch_lo.wire(11, ale);      // LE = ALE
        latch_lo.wire(10, gnd);
        latch_lo.wire(20, vcc);
        // D pins: 3,4,7,8,13,14,17,18  Q pins: 2,5,6,9,12,15,16,19
        latch_lo.wire(3, ad[0]); latch_lo.wire(2, la[0]);
        latch_lo.wire(4, ad[1]); latch_lo.wire(5, la[1]);
        latch_lo.wire(7, ad[2]); latch_lo.wire(6, la[2]);
        latch_lo.wire(8, ad[3]); latch_lo.wire(9, la[3]);
        latch_lo.wire(13, ad[4]); latch_lo.wire(12, la[4]);
        latch_lo.wire(14, ad[5]); latch_lo.wire(15, la[5]);
        latch_lo.wire(17, ad[6]); latch_lo.wire(16, la[6]);
        latch_lo.wire(18, ad[7]); latch_lo.wire(19, la[7]);
        latch_lo_ic = latch_lo.emplace<IC_74S373>();

        // U9: 74S373 Address Latch (mid byte: A0-A7 -> XA8-XA15)
        latch_mid.wire(1, aen_brd);  // ~OE = AEN_BRD
        latch_mid.wire(11, ale);     // LE = ALE
        latch_mid.wire(10, gnd);
        latch_mid.wire(20, vcc);
        latch_mid.wire(3, a_upper[0]); latch_mid.wire(2, la[8]);
        latch_mid.wire(4, a_upper[1]); latch_mid.wire(5, la[9]);
        latch_mid.wire(7, a_upper[2]); latch_mid.wire(6, la[10]);
        latch_mid.wire(8, a_upper[3]); latch_mid.wire(9, la[11]);
        latch_mid.wire(13, a_upper[4]); latch_mid.wire(12, la[12]);
        latch_mid.wire(14, a_upper[5]); latch_mid.wire(15, la[13]);
        latch_mid.wire(17, a_upper[6]); latch_mid.wire(16, la[14]);
        latch_mid.wire(18, a_upper[7]); latch_mid.wire(19, la[15]);
        latch_mid_ic = latch_mid.emplace<IC_74S373>();

        // U7: 74S373 Address Latch (high nibble: A8-A11 -> XA16-XA19)
        latch_hi.wire(1, aen_brd);   // ~OE = AEN_BRD
        latch_hi.wire(11, ale);      // LE = ALE
        latch_hi.wire(10, gnd);
        latch_hi.wire(20, vcc);
        latch_hi.wire(3, a_upper[8]);  latch_hi.wire(2, la[16]);
        latch_hi.wire(4, a_upper[9]);  latch_hi.wire(5, la[17]);
        latch_hi.wire(7, a_upper[10]); latch_hi.wire(6, la[18]);
        latch_hi.wire(8, a_upper[11]); latch_hi.wire(9, la[19]);
        // D4-D7 / Q4-Q7 unused on U7 -- pad with dummies for PinBlock<8> contiguity.
        latch_hi.wire(13, u7_d_pad[0]); latch_hi.wire(12, u7_q_pad[0]);
        latch_hi.wire(14, u7_d_pad[1]); latch_hi.wire(15, u7_q_pad[1]);
        latch_hi.wire(17, u7_d_pad[2]); latch_hi.wire(16, u7_q_pad[2]);
        latch_hi.wire(18, u7_d_pad[3]); latch_hi.wire(19, u7_q_pad[3]);
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
        io_decode.wire(11, pg_reg_cs);  // ~Y4 = page reg CS (0x80-0x9F)
        io_decode.wire(16, vcc);
        io_dec = io_decode.emplace<IC_74S138<0x1F>>();
        io_dec->set_g2b_async();  // ~G2B=~AEN from U98 (registered, breaks DAG cycle)

        // U2: 8259A PIC
        pic_socket.wire(1, intr_cs);
        pic_socket.wire(2, xiow);
        pic_socket.wire(3, xior);
        pic_socket.wire(4, xd7);  pic_socket.wire(5, xd6);
        pic_socket.wire(6, xd5);  pic_socket.wire(7, xd4);
        pic_socket.wire(8, xd3);  pic_socket.wire(9, xd2);
        pic_socket.wire(10, xd1); pic_socket.wire(11, xd0);
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
        nand_socket.wire(1, la[19]);            // A1 = A19 (pre-buffer)
        nand_socket.wire(2, la[18]);            // B1 = A18
        nand_socket.wire(4, la[17]);            // C1 = A17
        nand_socket.wire(5, la[16]);            // D1 = A16
        nand_socket.wire(6, rom_addr_sel);      // Y1 = ~ROM_ADDR_SEL
        nand_socket.wire(7, gnd);
        nand_socket.wire(8, n_000240);          // Y2 = N-000240 -> U82 (unimpl)
        nand_socket.wire(9, n_000239);          // A2 = N-000239 (from U84 gate 2)
        nand_socket.wire(10, n_000239);         // B2 = N-000239 (doubled)
        nand_socket.wire(12, xiow);             // C2 = ~XIOW
        nand_socket.wire(13, xior);             // D2 = ~XIOR
        nand_socket.wire(14, vcc);
        nand_ic = nand_socket.emplace<IC_74S20<0x03>>();

        // U46: 74S138 ROM Chip Select Decoder
        // Decodes A15:A13 into ~CS2-~CS7 when ~ROM_ADDR_SEL=Low and ~MEMR=Low.
        // G1 = VCC (always enabled -- on real board this is ~RESET_DRV,
        //           but 8288 doesn't issue ~MEMR during reset, so safe).
        rom_decode.wire(1, la[13]);             // A = A13 (pre-buffer)
        rom_decode.wire(2, la[14]);             // B = A14
        rom_decode.wire(3, la[15]);             // C = A15
        rom_decode.wire(4, xmemr);              // ~G2A = ~XMEMR (active during memory read)
        rom_decode.wire(5, rom_addr_sel);       // ~G2B = ~ROM_ADDR_SEL
        rom_decode.wire(6, vcc);                // G1 = VCC (see note above)
        rom_decode.wire(7, cs7);                // ~Y7 = ~CS7 -> U33 (FE000-FFFFF)
        // ~Y0-~Y6 unconnected (no other ROM chips installed in test bench)
        rom_decode.wire(8, gnd);
        rom_decode.wire(16, vcc);
        rom_dec = rom_decode.emplace<IC_74S138<0x80>>();

        // U33: 8K x 8 BIOS ROM (FE000-FFFFF)
        // Address pins wired to XA0-XA12, data pins to XD0-XD7.
        // XD reaches D through U13 (74S245 system data bus transceiver).
        rom_socket.wire(1, xa[7]);              // A7
        rom_socket.wire(2, xa[6]);              // A6
        rom_socket.wire(3, xa[5]);              // A5
        rom_socket.wire(4, xa[4]);              // A4
        rom_socket.wire(5, xa[3]);              // A3
        rom_socket.wire(6, xa[2]);              // A2
        rom_socket.wire(7, xa[1]);              // A1
        rom_socket.wire(8, xa[0]);              // A0
        rom_socket.wire(9, xd0);                // XD0
        rom_socket.wire(10, xd1);               // XD1
        rom_socket.wire(11, xd2);               // XD2
        rom_socket.wire(12, gnd);               // GND
        rom_socket.wire(13, xd3);               // XD3
        rom_socket.wire(14, xd4);               // XD4
        rom_socket.wire(15, xd5);               // XD5
        rom_socket.wire(16, xd6);               // XD6
        rom_socket.wire(17, xd7);               // XD7
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
        mux_lo.wire(2, la[0]);             // I0a = A0 (pre-buffer)
        mux_lo.wire(3, la[8]);             // I1a = A8
        mux_lo.wire(4, dram_ma0);          // Ya = MA0
        mux_lo.wire(5, la[1]);             // I0b = A1
        mux_lo.wire(6, la[9]);             // I1b = A9
        mux_lo.wire(7, dram_ma1);          // Yb = MA1
        mux_lo.wire(8, gnd);               // GND
        mux_lo.wire(9, dram_ma2);          // Yc = MA2
        mux_lo.wire(10, la[10]);           // I1c = A10
        mux_lo.wire(11, la[2]);            // I0c = A2
        mux_lo.wire(12, dram_ma3);         // Yd = MA3
        mux_lo.wire(13, la[11]);           // I1d = A11
        mux_lo.wire(14, la[3]);            // I0d = A3
        mux_lo.wire(15, gnd);              // ~STROBE = GND (always enabled)
        mux_lo.wire(16, vcc);              // VCC
        mux_lo_ic = mux_lo.emplace<IC_74S158>();

        // U79: 74S158 DRAM Address MUX (high nibble: MA4-MA7)
        mux_hi.wire(1, addr_sel);          // SELECT
        mux_hi.wire(2, la[4]);             // I0a = A4 (pre-buffer)
        mux_hi.wire(3, la[12]);            // I1a = A12
        mux_hi.wire(4, dram_ma4);          // Ya = MA4
        mux_hi.wire(5, la[5]);             // I0b = A5
        mux_hi.wire(6, la[13]);            // I1b = A13
        mux_hi.wire(7, dram_ma5);          // Yb = MA5
        mux_hi.wire(8, gnd);               // GND
        mux_hi.wire(9, dram_ma6);          // Yc = MA6
        mux_hi.wire(10, la[14]);           // I1c = A14
        mux_hi.wire(11, la[6]);            // I0c = A6
        mux_hi.wire(12, dram_ma7);         // Yd = MA7
        mux_hi.wire(13, la[15]);           // I1d = A15
        mux_hi.wire(14, la[7]);            // I0d = A7
        mux_hi.wire(15, gnd);              // ~STROBE = GND (always enabled)
        mux_hi.wire(16, vcc);              // VCC
        mux_hi_ic = mux_hi.emplace<IC_74S158>();
        mux_lo_ic->set_partner(mux_hi_ic);

        // U81: 74S00 NAND -- RAS/CAS generation + refresh gating.
        // Gate 1: DACK0 NAND RAS -> ~REFRSH_GATE
        // Gate 2: ~MEMR NAND ~MEMW -> RAS (fires when either memory cmd active)
        // Gate 3: N-000258 NAND N-000253 -> ~CAS (TD1 delayed RAS inputs)
        // Gate 4: unconnected (parity/channel check, N.P. signals)
        nand81_socket.wire(1, gnd);            // A1 = DACK0 (Low = no DMA)
        nand81_socket.wire(2, ras);            // B1 = RAS
        nand81_socket.wire(3, refrsh_gate);    // Y1 = ~REFRSH_GATE
        nand81_socket.wire(4, xmemr);          // A2 = ~XMEMR
        nand81_socket.wire(5, xmemw);          // B2 = ~XMEMW
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
        ram_range.wire(3, la[18]);             // C = A18 (pre-buffer)
        ram_range.wire(4, gnd);                // ~G2A = GND (always enabled)
        ram_range.wire(5, la[19]);             // ~G2B = A19 (Low for < 512K)
        ram_range.wire(6, dack0_brd);           // G1 = ~DACK_0_BRD (disables during DRAM refresh)
        ram_range.wire(8, gnd);
        ram_range.wire(15, ram_addr_sel);      // ~Y0 = ~RAM_ADDR_SEL
        ram_range.wire(16, vcc);
        ram_range_ic = ram_range.emplace<IC_74S138<0x01>>();

        // U65: 74S138 Per-Bank RAS Decoder
        // Decodes A16/A17 into bank selects, enabled by RAS + ~RAM_ADDR_SEL.
        // C=VCC so select range is 4-7 (outputs ~Y4-~Y7).
        ras_decode.wire(1, la[16]);            // A = A16 (pre-buffer)
        ras_decode.wire(2, la[17]);            // B = A17
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
        ras_dec = ras_decode.emplace<IC_74S138<0xF0>>();

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
        ras_gate_ic = ras_gate.emplace<IC_74S08<0xFF, true>>();

        // U47: 74S138 Per-Bank CAS Decoder
        // Decodes A16/A17 into per-bank ~CAS0-3, enabled by ~CAS + ~RAM_ADDR_SEL.
        cas_decode.wire(1, la[16]);            // A = A16 (pre-buffer)
        cas_decode.wire(2, la[17]);            // B = A17
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
        cas_dec = cas_decode.emplace<IC_74S138<0x0F>>();

        // U83: 74S04 Hex Inverter (~WE buffer)
        // Gates 1+2 double-invert ~MEMW to buffer it for DRAM ~WE fan-out.
        // ~XMEMW -> pin1 -> pin2 (inverted) -> pin3 -> pin4 (restored) -> ~WE
        inv_socket.wire(1, xmemw);         // A1 = ~XMEMW
        inv_socket.wire(2, u83_mid);       // Y1 = inverted ~MEMW
        inv_socket.wire(3, u83_mid);       // A2 = inverted ~MEMW
        inv_socket.wire(4, dram_we);       // Y2 = ~WE (double-inverted = buffered ~MEMW)
        inv_socket.wire(7, gnd);
        inv_socket.wire(8, n_000238);      // Y4 = bus idle grant (inverted N-000243)
        inv_socket.wire(9, n_000243);      // A4 = N-000243 (from U5 output)
        inv_socket.wire(10, n_000241);     // Y5 = N-000241 (inverted ~XMEMR)
        inv_socket.wire(11, xmemr);        // A5 = ~XMEMR
        inv_socket.wire(14, vcc);
        inv_ic = inv_socket.emplace<IC_74S04>();

        // U35: 8237A DMA Controller
        // Wired per BRD. Series termination resistors aliased.
        // HOLDA=GND prevents DMA transfers (no bus handshake logic yet).
        // The 8237A responds to I/O ports 0x00-0x0F via ~DMA_CS from U66.
        dma_socket.wire(1, xior);           // ~XIOR
        dma_socket.wire(2, xiow);          // ~XIOW
        dma_socket.wire(3, xmemr);         // ~XMEMR
        dma_socket.wire(4, xmemw);         // ~XMEMW
        dma_socket.wire(5, vcc);           // VCC
        dma_socket.wire(6, rdy_to_dma);    // RDY_TO_DMA from U97 gate 3
        dma_socket.wire(7, holda);         // HOLDA from U67 FF1
        dma_socket.wire(8, adstb);         // ADSTB (N-000280 aliased)
        dma_socket.wire(9, dma_aen_out);   // AEN output (unconnected on real 5150)
        dma_socket.wire(10, hrq);          // HRQ (N-000286 aliased)
        dma_socket.wire(11, dma_cs);       // ~DMA_CS (from U66)
        dma_socket.wire(12, dclk);         // DCLK from U52 gate 2
        dma_socket.wire(13, reset);        // RESET
        dma_socket.wire(14, dack2);        // ~DACK2
        dma_socket.wire(15, dack3);        // ~DACK3
        dma_socket.wire(16, drq3);         // DRQ3
        dma_socket.wire(17, drq2);         // DRQ2
        dma_socket.wire(18, drq1);         // DRQ1
        dma_socket.wire(19, drq0);         // DRQ0
        dma_socket.wire(20, gnd);          // GND
        dma_socket.wire(21, xd7);           // XD7
        dma_socket.wire(22, xd6);
        dma_socket.wire(23, xd5);
        dma_socket.wire(24, dack1);        // ~DACK1
        dma_socket.wire(25, dack0_brd);    // ~DACK_0_BRD
        dma_socket.wire(26, xd4);
        dma_socket.wire(27, xd3);
        dma_socket.wire(28, xd2);
        dma_socket.wire(29, xd1);
        dma_socket.wire(30, xd0);          // XD0
        dma_socket.wire(31, vcc);          // VCC
        dma_socket.wire(32, xa[0]);        // XA0
        dma_socket.wire(33, xa[1]);        // XA1
        dma_socket.wire(34, xa[2]);        // XA2
        dma_socket.wire(35, xa[3]);        // XA3
        dma_socket.wire(36, eop);          // ~EOP (N-000281 aliased)
        dma_socket.wire(37, dma_a4);       // A4 (N-000285 -> U17 pin 11)
        dma_socket.wire(38, dma_a5);       // A5 (N-000282 -> U17 pin 13)
        dma_socket.wire(39, dma_a6);       // A6 (N-000284 -> U17 pin 15)
        dma_socket.wire(40, dma_a7);       // A7 (N-000283 -> U17 pin 17)
        dma_ic = dma_socket.emplace<IC_8237A>();
        dma_ic->set_xcvr(xcvr, mem_xcvr, xcvr13_ic, xcvr14_ic);

        // --- DMA Bus Grant Handshake ---
        // Path: HRQ -> U99(inv) -> U52(NAND) -> U5(8-NAND) -> U83(inv)
        //       -> U98(FF) -> U67(FF) -> HOLDA -> 8237A
        //       -> U98(FF) -> AEN_BRD -> 74S373 ~OE + 8288 ~AEN

        // U51: 74S04 Hex Inverter (~RESET_DRV, ~WRT_DMA_PG_REG)
        // BRD: inv1: RESET -> ~RESET_DRV
        //       inv6: N-000261 -> ~WRT_DMA_PG_REG
        //
        // N-000261 = NOR(~XIOW, pg_reg_cs) from U50 gate 4.
        // To avoid a DAG cycle through U50, we generate N-000261 by wiring
        // U51 inv6 input to pg_reg_cs. Since both pg_reg_cs and ~XIOW must be
        // Low for a valid page register write, and pg_reg_cs already encodes
        // the address decode, using pg_reg_cs alone as the inv6 input preserves
        // the correct write timing (the OR with ~XIOW just adds an extra guard
        // that is always true when ~CS is active during an IOW cycle).
        // TODO: Wire U50 gate 4 properly once scheduler supports per-gate deps.
        inv51_socket.wire(1, reset);           // A1 = RESET
        inv51_socket.wire(2, reset_drv_bar);   // Y1 = ~RESET_DRV
        inv51_socket.wire(7, gnd);
        // inv6 removed: wrt_dma_pg now generated by U101 gate 3 = OR(~XIOW, pg_reg_cs)
        inv51_socket.wire(14, vcc);
        inv51_ic = inv51_socket.emplace<IC_74S04>();

        // U99: 74S04 Hex Inverter (HRQ, ~EOP, CLK88)
        // BRD: inv1: HRQ(N-000286) -> ~HRQ_DMA
        //       inv2: ~EOP(N-000281) -> T/C
        //       inv4: CLK88 -> N-000247 (~CLK88)
        inv99_socket.wire(1, hrq);             // A1 = HRQ (N-000286)
        inv99_socket.wire(2, hrq_dma_bar);     // Y1 = ~HRQ_DMA
        inv99_socket.wire(3, eop);             // A2 = ~EOP (N-000281)
        inv99_socket.wire(4, tc);              // Y2 = T/C
        inv99_socket.wire(7, gnd);
        inv99_socket.wire(8, nclk88);          // Y4 = N-000247 (~CLK88)
        inv99_socket.wire(9, clk);             // A4 = CLK88 (= CLK in test bench)
        inv99_socket.wire(14, vcc);
        inv99_ic = inv99_socket.emplace<IC_74S04>();

        // U52: 74S00 Quad NAND
        // BRD: gate1: N-000227 NAND ~HRQ_DMA -> N-000242
        //       gate2: N-000247 NAND N-000249 -> DCLK
        // N-000227 is pulled up via RN1 (= VCC). N-000249 = VCC (RC timing stub).
        nand52_socket.wire(1, vcc);            // A1 = N-000227 (pull-up, always High)
        nand52_socket.wire(2, hrq_dma_bar);    // B1 = ~HRQ_DMA
        nand52_socket.wire(3, n_000242);       // Y1 = N-000242
        nand52_socket.wire(4, nclk88);         // A2 = N-000247 (~CLK88)
        nand52_socket.wire(5, vcc);            // B2 = N-000249 (RC timing, stub VCC)
        nand52_socket.wire(6, dclk);           // Y2 = DCLK = ~(~CLK88 & VCC) = CLK88
        nand52_socket.wire(7, gnd);
        nand52_socket.wire(14, vcc);
        nand52_ic = nand52_socket.emplace<IC_74S00>();

        // U5: 74LS30 8-Input NAND (bus idle detect)
        // BRD: all 8 inputs must be High for output Low (bus idle + HRQ active)
        // Inputs: +5V(1), +5V(2), U5.3(3), N-000242(4), ~LOCK(5), ~S1(6), ~S0(11), ~S2(12)
        // U5.3 = N-000291 = OR(~DMA_CS, ~XIOW) from U101 gate 4.
        nand5_socket.wire(1, vcc);             // A = +5V
        nand5_socket.wire(2, vcc);             // B = +5V
        nand5_socket.wire(3, u101_y4);         // C = U101 gate 4 output
        nand5_socket.wire(4, n_000242);        // D = N-000242 (HRQ active flag)
        nand5_socket.wire(5, cpu_lock);        // E = ~LOCK
        nand5_socket.wire(6, s1);              // F = ~S1
        nand5_socket.wire(7, gnd);
        nand5_socket.wire(8, n_000243);        // Y = N-000243 (Low when bus idle)
        nand5_socket.wire(11, s0);             // G = ~S0
        nand5_socket.wire(12, s2);             // H = ~S2
        nand5_socket.wire(14, vcc);
        nand5_ic = nand5_socket.emplace<IC_74LS30>();

        // U98: 74S175 Quad D Flip-Flop (AEN_BRD generation)
        // BRD: CLK=CLK(9), ~MR=~RESET_DRV(1)
        // FF1: D=HOLDA(4) -> Q=AEN_BRD(2), ~Q=~AEN(3)
        // FF2: D=AEN_BRD(5) -> Q=N-000245(7), ~Q=~DMA_WAIT(6)
        // FF3: D=~RDY~/WAIT(12) -> Q=N-000244(11), ~Q=nc(10)
        // FF4: D=N-000238(13) -> Q=N-000231(15), ~Q=nc(14)
        ff98_socket.wire(1, reset_drv_bar);    // ~CLR = ~RESET_DRV
        ff98_socket.wire(2, aen_brd);          // 1Q = AEN_BRD
        ff98_socket.wire(3, aen_bar);          // ~1Q = ~AEN (drives U6.15, U66.6)
        ff98_socket.wire(4, holda);            // 1D = HOLDA
        ff98_socket.wire(5, aen_brd);          // 2D = AEN_BRD (latches AEN_BRD again)
        ff98_socket.wire(6, dma_wait_bar);     // ~2Q = ~DMA_WAIT
        ff98_socket.wire(7, n_000245);         // 2Q = N-000245 -> U97 gate 4
        ff98_socket.wire(8, gnd);              // GND
        ff98_socket.wire(9, clk);              // CLK
        // Pin 10 (~3Q) unconnected per BRD
        ff98_socket.wire(11, n_000244);        // 3Q = N-000244 -> U97 gate 3
        ff98_socket.wire(12, rdy_wait_bar);    // 3D = ~RDY~/WAIT (from U82)
        ff98_socket.wire(13, n_000238);        // 4D = N-000238 (bus idle grant from U83)
        // Pin 14 (~4Q) unconnected per BRD
        ff98_socket.wire(15, n_000231);        // 4Q = N-000231 -> U67 FF1 D
        ff98_socket.wire(16, vcc);             // VCC
        ff98_ic = ff98_socket.emplace<IC_74S175>();
        ff98_ic->set_d_sync(0);  // D1=HOLDA: sync so U98 evaluates after U67

        // U67: 74S74 Dual D Flip-Flop (HOLDA + DRQ0 latch)
        // FF1: generates HOLDA from bus idle grant
        //   ~CLR=N-000242, D=N-000231, CLK=N-000247(~CLK88),
        //   ~PRE=N-000230(=~Q feedback), Q=HOLDA, ~Q=N-000230
        // FF2: DRQ0 latch (PIT CH1 -> DREQ0)
        //   ~CLR=~DACK_0_BRD, D=VCC, CLK=N-000328(PIT OUT1),
        //   ~PRE=VCC, Q=DRQ0
        ff67_socket.wire(1, n_000242);         // ~CLR1 = N-000242 (cleared when HRQ inactive)
        ff67_socket.wire(2, n_000231);         // D1 = N-000231 (bus idle, delayed by U98)
        ff67_socket.wire(3, nclk88);           // CLK1 = N-000247 (~CLK88)
        ff67_socket.wire(4, n_000230);         // ~PRE1 = N-000230 (~Q1 feedback)
        ff67_socket.wire(5, holda);            // Q1 = HOLDA
        ff67_socket.wire(6, n_000230);         // ~Q1 = N-000230 -> ~PRE1 feedback
        ff67_socket.wire(7, gnd);              // GND
        // FF2: DRQ0 latch
        ff67_socket.wire(9, drq0);             // Q2 = DRQ0
        ff67_socket.wire(10, vcc);             // ~PRE2 = VCC (no preset)
        ff67_socket.wire(11, n_000328);        // CLK2 = N-000328 (PIT OUT1, stub)
        ff67_socket.wire(12, vcc);             // D2 = VCC (always latch High)
        ff67_socket.wire(13, dack0_brd);       // ~CLR2 = ~DACK_0_BRD (clears on DMA ack)
        ff67_socket.wire(14, vcc);             // VCC
        ff67_ic = ff67_socket.emplace<IC_74S74>(true);  // async CLK: FF2 CLK=PIT OUT1 (async timer)

        // U82: 74S74 Dual D Flip-Flop (keyboard IRQ1 + DMA wait state)
        // FF1: keyboard/SW1 mux control (not implemented -- tie outputs stable)
        ff82_socket.wire(1, vcc);              // ~CLR1 = VCC (no clear)
        ff82_socket.wire(2, gnd);              // D1 = GND (stub)
        ff82_socket.wire(3, gnd);              // CLK1 = GND (stub, never clocks)
        ff82_socket.wire(4, vcc);              // ~PRE1 = VCC (no preset)
        ff82_socket.wire(5, gnd);              // Q1 = IRQ1 (stub, not connected to PIC yet)
        ff82_socket.wire(6, gnd);              // ~Q1 = stub
        ff82_socket.wire(7, gnd);              // GND
        // FF2: DMA wait state generator
        //   D = ~DACK_0_BRD, CLK = N-000240, ~CLR = N-000244
        //   Q = ~RDY/WAIT -> U11 pin 3, ~Q = N-000237 -> U97 pin 9
        ff82_socket.wire(8, n_000237);         // ~Q2 = N-000237
        ff82_socket.wire(9, rdy_wait_bar);     // Q2 = ~RDY/WAIT
        ff82_socket.wire(10, io_ch_rdy);       // ~PRE2 = I/O_CH_RDY
        ff82_socket.wire(11, n_000240);        // CLK2 = N-000240 (from U64)
        ff82_socket.wire(12, dack0_brd);       // D2 = ~DACK_0_BRD
        ff82_socket.wire(13, n_000244);        // ~CLR2 = N-000244 (from U98)
        ff82_socket.wire(14, vcc);             // VCC
        ff82_ic = ff82_socket.emplace<IC_74S74>(true);  // async CLK (registered feedback)

        // U18: 74S373 DMA Page Address Latch (A8-A15 from data bus via ADSTB)
        // BRD: ~OC=~DMA_AEN, LE=ADSTB(N-000280)
        // During DMA transfer, 8237A multiplexes upper address onto data bus,
        // then strobes ADSTB to latch A8-A15 into U18.
        dma_page_latch.wire(1, dma_aen_bar);   // ~OE = ~DMA_AEN (stub VCC = disabled)
        dma_page_latch.wire(2, la[8]);          // Q0 = A8 (output to pre-buffer address bus)
        dma_page_latch.wire(3, xd0);            // D0 = XD0 (from X-side data bus)
        dma_page_latch.wire(4, xd1);            // D1 = XD1
        dma_page_latch.wire(5, la[9]);          // Q1 = A9
        dma_page_latch.wire(6, la[10]);         // Q2 = A10
        dma_page_latch.wire(7, xd2);            // D2 = XD2
        dma_page_latch.wire(8, xd3);            // D3 = XD3
        dma_page_latch.wire(9, la[11]);         // Q3 = A11
        dma_page_latch.wire(10, gnd);           // GND
        dma_page_latch.wire(11, adstb);         // LE = ADSTB (N-000280)
        dma_page_latch.wire(12, la[12]);        // Q4 = A12
        dma_page_latch.wire(13, xd4);           // D4 = XD4
        dma_page_latch.wire(14, xd5);           // D5 = XD5
        dma_page_latch.wire(15, la[13]);        // Q5 = A13
        dma_page_latch.wire(16, la[14]);        // Q6 = A14
        dma_page_latch.wire(17, xd6);           // D6 = XD6
        dma_page_latch.wire(18, xd7);           // D7 = XD7
        dma_page_latch.wire(19, la[15]);        // Q7 = A15
        dma_page_latch.wire(20, vcc);           // VCC
        {   // U18 D inputs are async: latched by ADSTB pulse, not combinational D->Q.
            auto ic = std::make_unique<IC_74S373>();
            ic->set_async_inputs();
            dma_page_latch_ic = ic.get();
            ic->set_name("U18-74S373");
            ic->install(dma_page_latch);
            // Q outputs only driven when ~OE(~DMA_AEN)=Low (DMA mode).
            // During CPU mode (~DMA_AEN=High), Q is tri-stated -- no DAG edges.
            static constexpr int q_pins[] = {2, 5, 6, 9, 12, 15, 16, 19};
            Pin qp[8];
            for (int i = 0; i < 8; ++i)
                qp[i] = dma_page_latch.pin_signal(q_pins[i])->pin();
            auto* raw = ic.get();
            ic->declare_bidir_block({qp[0],qp[1],qp[2],qp[3],qp[4],qp[5],qp[6],qp[7]},
                Component::BidirDir::HiZ | Component::BidirDir::Output,
                [raw]() {
                    return (raw->dma_output() || raw->oe_level() == Level::Low)
                        ? Component::BidirDir::Output : Component::BidirDir::HiZ;
                });
            dma_page_latch.insert(std::move(ic));
        }

        // U19: 74LS670 4x4 Register File (DMA page register, A16-A19)
        // BRD: write by CPU via I/O port, read by DMA channel via ~DACK2/~DACK3
        dma_page_reg.wire(1, xd1);              // D1 = XD1
        dma_page_reg.wire(2, xd2);              // D2 = XD2
        dma_page_reg.wire(3, xd3);              // D3 = XD3
        dma_page_reg.wire(4, dack2);            // RA = ~DACK2 (read address A)
        dma_page_reg.wire(5, dack3);            // RB = ~DACK3 (read address B)
        dma_page_reg.wire(6, la[19]);           // Q3 = A19 (pre-buffer)
        dma_page_reg.wire(7, la[18]);           // Q2 = A18
        dma_page_reg.wire(8, gnd);              // GND
        dma_page_reg.wire(9, la[17]);           // Q1 = A17
        dma_page_reg.wire(10, la[16]);          // Q0 = A16
        dma_page_reg.wire(11, dma_aen_bar);     // ~RE = ~DMA_AEN (stub VCC = disabled)
        dma_page_reg.wire(12, wrt_dma_pg);      // ~WE = ~WRT_DMA_PG_REG (from U101 gate 1)
        dma_page_reg.wire(13, xa[1]);           // WB = XA1
        dma_page_reg.wire(14, xa[0]);           // WA = XA0
        dma_page_reg.wire(15, xd0);             // D0 = XD0
        dma_page_reg.wire(16, vcc);             // VCC
        {   // U19 D inputs are async: written by ~WE pulse, not combinational D->Q.
            auto ic = std::make_unique<IC_74LS670>();
            ic->set_async_inputs();
            dma_page_reg_ic = ic.get();
            ic->set_name("U19-74LS670");
            ic->install(dma_page_reg);
            dma_page_reg.insert(std::move(ic));
        }

        // Wire DMA address latches to 8237A (must be after U18/U19 creation)
        dma_ic->set_addr_latches(dma_page_latch_ic, dma_page_reg_ic);
        bc->set_addr_latches(dma_page_latch_ic, dma_page_reg_ic);

        // U17: 74S244 DMA Address Buffer (XA0-XA3 -> A0-A3, DMA A4-A7 -> A4-A7)
        // BRD: Group 1 (~1G=~DMA_AEN): XA0-XA3 (from 8237A) -> A0-A3 (system addr bus)
        //       Group 2 (~2G=~DMA_AEN): N-000285/282/284/283 (DMA A4-A7) -> A4-A7
        // During CPU mode (~DMA_AEN=High), both groups tri-stated -- U16 drives XA.
        // During DMA mode (~DMA_AEN=Low), both groups enabled -- buffers DMA addr to A bus.
        buf17_socket.wire(1, dma_aen_bar);       // ~1G = ~DMA_AEN
        buf17_socket.wire(2, xa[0]);             // 1A1 = XA0 (from 8237A pin 32)
        buf17_socket.wire(3, la[7]);             // 2Y4 = A7 (output to system addr bus)
        buf17_socket.wire(4, xa[1]);             // 1A2 = XA1
        buf17_socket.wire(5, la[6]);             // 2Y3 = A6
        buf17_socket.wire(6, xa[2]);             // 1A3 = XA2
        buf17_socket.wire(7, la[5]);             // 2Y2 = A5
        buf17_socket.wire(8, xa[3]);             // 1A4 = XA3
        buf17_socket.wire(9, la[4]);             // 2Y1 = A4
        buf17_socket.wire(10, gnd);
        buf17_socket.wire(11, dma_a4);            // 2A1 = N-000285 (DMA A4)
        buf17_socket.wire(12, la[3]);            // 1Y4 = A3 (output to system addr bus)
        buf17_socket.wire(13, dma_a5);            // 2A2 = N-000282 (DMA A5)
        buf17_socket.wire(14, la[2]);            // 1Y3 = A2
        buf17_socket.wire(15, dma_a6);            // 2A3 = N-000284 (DMA A6)
        buf17_socket.wire(16, la[1]);            // 1Y2 = A1
        buf17_socket.wire(17, dma_a7);            // 2A4 = N-000283 (DMA A7)
        buf17_socket.wire(18, la[0]);            // 1Y1 = A0
        buf17_socket.wire(19, dma_aen_bar);      // ~2G = ~DMA_AEN
        buf17_socket.wire(20, vcc);
        buf17_ic = buf17_socket.emplace<IC_74S244>();

        // U16: 74S244 Address Buffer (LA0-LA7 -> XA0-XA7)
        // BRD: Group 1 (~1G=AEN_BRD): LA7,LA6,LA5,LA4 -> XA7,XA6,XA5,XA4
        //       Group 2 (~2G=AEN_BRD): LA3,LA2,LA1,LA0 -> XA3,XA2,XA1,XA0
        // Enabled during CPU mode (AEN_BRD=Low). Tri-stated during DMA (U17 drives).
        buf16_socket.wire(1, aen_brd);           // ~1G = AEN_BRD (Low=enabled during CPU)
        buf16_socket.wire(2, la[7]);             // 1A1 = LA7 (input from latch)
        buf16_socket.wire(3, xa[0]);             // 2Y4 = XA0 (output)
        buf16_socket.wire(4, la[6]);             // 1A2 = LA6
        buf16_socket.wire(5, xa[1]);             // 2Y3 = XA1
        buf16_socket.wire(6, la[5]);             // 1A3 = LA5
        buf16_socket.wire(7, xa[2]);             // 2Y2 = XA2
        buf16_socket.wire(8, la[4]);             // 1A4 = LA4
        buf16_socket.wire(9, xa[3]);             // 2Y1 = XA3
        buf16_socket.wire(10, gnd);
        buf16_socket.wire(11, la[3]);            // 2A1 = LA3
        buf16_socket.wire(12, xa[4]);            // 1Y4 = XA4
        buf16_socket.wire(13, la[2]);            // 2A2 = LA2
        buf16_socket.wire(14, xa[5]);            // 1Y3 = XA5
        buf16_socket.wire(15, la[1]);            // 2A3 = LA1
        buf16_socket.wire(16, xa[6]);            // 1Y2 = XA6
        buf16_socket.wire(17, la[0]);            // 2A4 = LA0
        buf16_socket.wire(18, xa[7]);            // 1Y1 = XA7
        buf16_socket.wire(19, aen_brd);          // ~2G = AEN_BRD
        buf16_socket.wire(20, vcc);
        buf16_ic = buf16_socket.emplace<IC_74S244>();

        // U15: 74S244 Address Buffer (LA8-LA12, misc control signals)
        // BRD: Group 1 (~1G=GND, always enabled): LA8-LA11 -> XA8-XA11
        //       Group 2 (~2G=GND, always enabled): LA12->XA12, CLK88, AEN_BRD, ~DACK_0_BRD
        buf15_socket.wire(1, gnd);               // ~1G = GND (always enabled)
        buf15_socket.wire(2, la[8]);             // 1A1 = LA8 (input from latch)
        buf15_socket.wire(3, isa_dack0);         // 2Y4 = ISA ~DACK0 (buffered)
        buf15_socket.wire(4, la[9]);             // 1A2 = LA9
        buf15_socket.wire(5, isa_aen);           // 2Y3 = ISA AEN (buffered)
        buf15_socket.wire(6, la[10]);            // 1A3 = LA10
        buf15_socket.wire(7, isa_clk88);         // 2Y2 = N-000289 (buffered CLK88 for ISA)
        buf15_socket.wire(8, la[11]);            // 1A4 = LA11
        buf15_socket.wire(9, xa[12]);            // 2Y1 = XA12
        buf15_socket.wire(10, gnd);
        buf15_socket.wire(11, la[12]);           // 2A1 = LA12
        buf15_socket.wire(12, xa[11]);           // 1Y4 = XA11
        buf15_socket.wire(13, clk);              // 2A2 = CLK88
        buf15_socket.wire(14, xa[10]);           // 1Y3 = XA10
        buf15_socket.wire(15, aen_brd);          // 2A3 = AEN_BRD
        buf15_socket.wire(16, xa[9]);            // 1Y2 = XA9
        buf15_socket.wire(17, dack0_brd);        // 2A4 = ~DACK_0_BRD
        buf15_socket.wire(18, xa[8]);            // 1Y1 = XA8
        buf15_socket.wire(19, gnd);              // ~2G = GND (always enabled)
        buf15_socket.wire(20, vcc);
        buf15_ic = buf15_socket.emplace<IC_74S244>();

        // --- ISA Slots (J1-J5) ---
        // All 5 slots share identical wiring (parallel bus).
        for (auto& slot : isa_slots) {
            // Data bus: SD0-SD7 = D0-D7 (system data bus, per BRD J5 pinout).
            // ISA slots connect directly to D bus, not XD.  U13 (D<->XD)
            // is disabled during DMA (AEN_BRD=HIGH), so ISA devices must
            // drive D directly for DMA writes to reach DRAM via U12 (D<->MD).
            slot.wire_pin(2, &d7);  slot.wire_pin(3, &d6);
            slot.wire_pin(4, &d5);  slot.wire_pin(5, &d4);
            slot.wire_pin(6, &d3);  slot.wire_pin(7, &d2);
            slot.wire_pin(8, &d1);  slot.wire_pin(9, &d0);

            // Address bus: SA0-SA19 from LA (latch outputs = A bus).
            // On the real 5150, A0-A19 is a single bus shared by the
            // 74S373 latches, 74S244 buffers, muxes, and ISA slots.
            for (int a = 0; a < 20; ++a)
                slot.wire_pin(31 - a, &la[a]);

            // Control signals: system-side per BRD (J5 nets are ~IOR/~IOW/~MEMR/~MEMW,
            // not the X-side versions).  U14 routes DMA commands to system side.
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

            // AEN_BRD on B28
            slot.wire_pin(59, &aen_brd);   // AEN_BRD from U98

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
            slot.wire_pin(58, &tc);        // T/C (B27) = inverted ~EOP from U99

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

        // U101: 74LS32 Quad OR
        // Gate 4: OR(~DMA_CS, ~XIOW) -> U5 pin 3
        // Gate 1: OR(~Y4, ~XIOW) -> ~WRT_DMA_PG_REG (Low when writing to 0x80-0x9F)
        //   On real board this goes through U50/U51, but OR gives same result.
        // U101: 74LS32 Quad OR
        // BRD: Gate 1 (1,2->3): OR(~XIOW, ~PIT_CS) -> PIT ~WR (U34 pin 23)
        //       Gate 2 (4,5->6): OR(~XIOR, ~PIT_CS) -> PIT ~RD (U34 pin 22)
        //       Gate 4 (12,13->11): OR(~DMA_CS, ~XIOW) -> U5 pin 3
        or101_socket.wire(1, xiow);            // A1 = ~XIOW
        or101_socket.wire(2, pit_cs);          // B1 = ~PIT_CS
        or101_socket.wire(3, pit_wr);          // Y1 = PIT ~WR
        or101_socket.wire(4, xior);            // A2 = ~XIOR
        or101_socket.wire(5, pit_cs);          // B2 = ~PIT_CS
        or101_socket.wire(6, pit_rd);          // Y2 = PIT ~RD
        or101_socket.wire(7, gnd);
        or101_socket.wire(9, xiow);            // A3 = ~XIOW
        or101_socket.wire(10, pg_reg_cs);      // B3 = ~Y4 (page reg CS)
        or101_socket.wire(8, wrt_dma_pg);      // Y3 = ~WRT_DMA_PG_REG = OR(~XIOW, pg_reg_cs)
        or101_socket.wire(12, dma_cs);         // A4 = ~DMA_CS
        or101_socket.wire(13, xiow);           // B4 = ~XIOW
        or101_socket.wire(11, u101_y4);        // Y4 = -> U5 pin 3
        or101_socket.wire(14, vcc);
        or101_ic = or101_socket.emplace<IC_74LS32>();

        // Gate 4 output (N-000291) feeds U5 which loops back through
        // U83->U84->U64->U27->U66->U101.  In CPU mode ~DMA_CS is High,
        // so OR(High, ~XIOW) = High (constant).  Sense ~DMA_CS externally:
        // HiZ when inactive, Output when DMA chip-selected.
        {
            Pin dma_cs_pin = dma_cs.pin();
            Pin y4_pin = u101_y4.pin();
            using BD = Component::BidirDir;
            or101_ic->declare_bidir_block({y4_pin},
                BD::HiZ | BD::Output,
                [dma_cs_pin]() -> BD {
                    return dma_cs_pin.level() == Level::Low
                        ? BD::Output : BD::HiZ;
                });
        }

        // U50: 74S02 Quad NOR (~DMA_AEN, ~WRT_DMA_PG_REG path)
        // BRD: Gate 1 (2,3->1): NOR(N-000246, N-000246) = ~N-000246 -> ~DMA_AEN
        //       Gate 4 (11,12->13): NOR(~XIOW, N-000259) -> N-000261 -> U51 inv6
        //
        // Gate 1 and gate 4 are independent on the real chip, but the scheduler
        // treats the whole IC as one node. Gate 4's input (~XIOW from U14) would
        // create a false cycle: U14->~XIOW->U50->~DMA_AEN->U14.
        // Split: gate 1 stays on U50. Gate 4 goes to U51 as a direct OR shortcut
        // (INV(NOR(A,B)) = OR(A,B), and U51 inv6 already inverts the result).
        //
        // Gate 1: ~DMA_AEN = NOR(N-000246, N-000246) = ~N-000246
        nor50_socket.wire(1, dma_aen_bar);     // Y1 = ~DMA_AEN
        nor50_socket.wire(2, n_000246);        // A1 = N-000246
        nor50_socket.wire(3, n_000246);        // B1 = N-000246
        nor50_socket.wire(7, gnd);
        // Gate 4 wired below via shortcut on U51.
        nor50_socket.wire(14, vcc);
        nor50_ic = nor50_socket.emplace<IC_74LS02>();

        // U84: 74S10 Triple 3-Input NAND (DACK/AEN gating)
        // BRD: Gate 1 (1,2,13->12): NAND(VCC, VCC, DT/~R) = ~(DT/~R) -> N-000215
        //       Gate 2 (3,4,5->6): NAND(~DACK_0_BRD, AEN_BRD, N-000241) -> N-000239
        //       Gate 3 (9,10,11->8): NAND(N-000236, ~PCK, N-000234) -> N-000235
        // N-000241, N-000236, N-000234 come from unimplemented ICs; stub to GND
        // so NAND outputs stay High (safe for NMI inactive).
        nand84_socket.wire(1, vcc);            // A1 = VCC
        nand84_socket.wire(2, vcc);            // B1 = VCC
        nand84_socket.wire(13, dtr);           // C1 = DT/~R
        nand84_socket.wire(12, n_000215);      // Y1 = N-000215
        nand84_socket.wire(3, dack0_brd);      // A2 = ~DACK_0_BRD
        nand84_socket.wire(4, aen_brd);        // B2 = AEN_BRD
        nand84_socket.wire(5, n_000241);       // C2 = N-000241 (inverted ~XMEMR from U83)
        nand84_socket.wire(6, n_000239);       // Y2 = N-000239
        nand84_socket.wire(9, gnd);            // A3 = N-000236 (stub GND)
        nand84_socket.wire(10, gnd);           // B3 = ~PCK (stub GND)
        nand84_socket.wire(11, gnd);           // C3 = N-000234 (stub GND)
        nand84_socket.wire(8, n_000235);       // Y3 = N-000235 -> U97 gate 1
        nand84_socket.wire(7, gnd);
        nand84_socket.wire(14, vcc);
        nand84_ic = nand84_socket.emplace<IC_74S10>();

        // U97: 74S08 Quad AND (NMI gate, RDY_TO_DMA)
        // BRD: Gate 1 (1,2->3): AND(N-000235, N-000225) -> NMI
        //       Gate 3 (9,10->8): AND(N-000237, N-000244) -> RDY_TO_DMA
        //       Gate 4 (12,13->11): AND(N-000245, AEN_BRD) -> N-000246
        // N-000225 = parity check (stub GND -> NMI stays Low = inactive)
        // N-000237 = VCC (pulled up on real board, keeps RDY_TO_DMA driven by U98 ~3Q)
        and97_socket.wire(1, n_000235);        // A1 = N-000235 (from U84 gate 3)
        and97_socket.wire(2, n_000225);        // B1 = N-000225 (parity stub, PSU drives for NMI)
        and97_socket.wire(3, nmi);             // Y1 = NMI
        and97_socket.wire(6, u97_y2_nc);        // Y2 = dummy (gate 2 unused)
        and97_socket.wire(7, gnd);
        and97_socket.wire(9, n_000237);        // A3 = N-000237 (from U82 FF2 ~Q)
        and97_socket.wire(10, n_000244);       // B3 = N-000244 (U98 ~3Q)
        and97_socket.wire(8, rdy_to_dma);      // Y3 = RDY_TO_DMA -> DMA pin 6
        and97_socket.wire(12, n_000245);       // A4 = N-000245 (U98 3Q)
        and97_socket.wire(13, aen_brd);        // B4 = AEN_BRD
        and97_socket.wire(11, n_000246);       // Y4 = N-000246
        and97_socket.wire(14, vcc);
        and97_ic = and97_socket.emplace<IC_74S08<0xE3>>();

        // U27: 74LS02 Quad NOR (ROM/RAM/IO select decode)
        // BRD: Gate 1 (2,3->1): NOR(N-000303, N-000288) -> N-000290 (U13 DIR)
        //       Gate 2 (5,6->4): NOR(~ROM_ADDR_SEL, ~XMEMR) -> N-000288
        //       Gate 3 (8,9->10): NOR(~RAM_ADDR_SEL, N-000304) -> N-000317
        //       Gate 4 (11,12->13): NOR(XA9, ~XIOR) -> N-000303
        nor27_socket.wire(1, n_000290);        // Y1 = N-000290 (-> U13 DIR)
        nor27_socket.wire(2, n_000303);        // A1 = N-000303
        nor27_socket.wire(3, n_000288);        // B1 = N-000288
        nor27_socket.wire(4, n_000288);        // Y2 = N-000288
        nor27_socket.wire(5, rom_addr_sel);    // A2 = ~ROM_ADDR_SEL
        nor27_socket.wire(6, xmemr);           // B2 = ~XMEMR
        nor27_socket.wire(7, gnd);
        nor27_socket.wire(8, ram_addr_sel);    // 3A = ~RAM_ADDR_SEL (input)
        nor27_socket.wire(9, n_000304);        // 3B = N-000304 (~XMEMW net alias, input)
        nor27_socket.wire(10, n_000317);       // 3Y = N-000317 (output)
        nor27_socket.wire(11, xa[9]);          // 4A = XA9 (input)
        nor27_socket.wire(12, xior);           // 4B = ~XIOR (input)
        nor27_socket.wire(13, n_000303);       // 4Y = N-000303 (output)
        nor27_socket.wire(14, vcc);
        nor27_ic = nor27_socket.emplace<IC_74LS02>();

        // U26: 74S175 Quad D Flip-Flop (PCLK divider)
        // FF2: ~Q->D feedback creates toggle FF. CLK=PCLK -> Q=PCLK/2=1.193 MHz.
        // FF0+FF1: keyboard sync (not used in test bench).
        // FF3: unused.
        ff26_socket.wire(1, reset_drv_bar);    // ~MR = ~RESET_DRV
        ff26_socket.wire(8, gnd);              // GND
        ff26_socket.wire(9, pclk);             // CLK = PCLK (2.386 MHz)
        ff26_socket.wire(10, pit_clk);         // 3Q = PIT_CLK (1.193 MHz)
        ff26_socket.wire(11, pclk_div2_fb);    // ~3Q = feedback
        ff26_socket.wire(12, pclk_div2_fb);    // 3D = ~3Q (toggle feedback)
        ff26_socket.wire(16, vcc);             // VCC
        ff26_ic = ff26_socket.emplace<IC_74S175>();

        // U34: 8253-5 PIT (Programmable Interval Timer)
        // Three 16-bit counters, all clocked at 1.193 MHz from U26.
        // D0-D7: pin 8=D0, pin 7=D1, ..., pin 1=D7 (reversed order).
        // OUT0->IRQ0 (timer tick), OUT1->DRQ0 latch CLK, OUT2->speaker (stub).
        pit_socket.wire(1, xd7);               // XD7
        pit_socket.wire(2, xd6);               // XD6
        pit_socket.wire(3, xd5);               // XD5
        pit_socket.wire(4, xd4);               // XD4
        pit_socket.wire(5, xd3);               // XD3
        pit_socket.wire(6, xd2);               // XD2
        pit_socket.wire(7, xd1);               // XD1
        pit_socket.wire(8, xd0);               // XD0
        pit_socket.wire(9, pit_clk);           // CLK0 = 1.193 MHz
        pit_socket.wire(10, irq0);             // OUT0 -> IRQ0
        pit_socket.wire(11, vcc);              // GATE0 = +5V (always enabled)
        pit_socket.wire(12, gnd);              // GND
        pit_socket.wire(13, n_000328);         // OUT1 -> U67 FF2 CLK (DRQ0 latch)
        pit_socket.wire(14, vcc);              // GATE1 = +5V (always enabled)
        pit_socket.wire(15, pit_clk);          // CLK1 = 1.193 MHz
        pit_socket.wire(16, vcc);              // GATE2 = +5V (stub, real: PPI PB0)
        // Pin 17 (OUT2) left unwired -- speaker output, not used in test bench.
        pit_socket.wire(18, pit_clk);          // CLK2 = 1.193 MHz
        pit_socket.wire(19, xa[0]);            // A0 = XA0
        pit_socket.wire(20, xa[1]);            // A1 = XA1
        pit_socket.wire(21, pit_cs);           // ~CS = ~PIT_CS (from U66 ~Y2)
        pit_socket.wire(22, pit_rd);           // ~RD = OR(~XIOR, ~PIT_CS) from U101 gate 2
        pit_socket.wire(23, pit_wr);           // ~WR = OR(~XIOW, ~PIT_CS) from U101 gate 1
        pit_socket.wire(24, vcc);              // VCC
        pit_ic = pit_socket.emplace<IC_8253>();
    }

    void register_all(Scheduler& scheduler) {
        // DMA group: skip when HRQ is Low and no DRQ pending.
        int dma_group = scheduler.register_group("DMA", [this]() {
            return dma_enabled;
        });

        scheduler.register_callback(xcvr);
        scheduler.register_callback(xcvr13_ic);
        scheduler.register_callback(xcvr14_ic);
        scheduler.register_callback(mem_xcvr);
        scheduler.register_callback(buf15_ic);
        scheduler.register_callback(buf16_ic);
        scheduler.register_callback(buf17_ic);
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
        scheduler.register_callback(dma_ic,             dma_group);
        scheduler.register_callback(inv99_ic,           dma_group);
        scheduler.register_callback(nand52_ic,          dma_group);
        scheduler.register_callback(nand5_ic,           dma_group);
        scheduler.register_callback(ff67_ic,            dma_group);
        scheduler.register_callback(ff82_ic,            dma_group);
        scheduler.register_callback(ff98_ic,            dma_group);
        scheduler.register_callback(inv51_ic,           dma_group);
        scheduler.register_callback(dma_page_latch_ic,  dma_group);
        scheduler.register_callback(dma_page_reg_ic,    dma_group);
        scheduler.register_callback(nand84_ic);
        scheduler.register_callback(and97_ic);
        scheduler.register_callback(nor27_ic);
        scheduler.register_callback(or101_ic);
        scheduler.register_callback(nor50_ic);
        scheduler.register_callback(ff26_ic);
        scheduler.register_callback(pit_ic);
        scheduler.register_callback(&dram);
        scheduler.register_callback(bc);
        scheduler.register_callback(pic);
        scheduler.register_fiber(cpu);
        scheduler.register_visual(clk_gen);
        clk_gen->set_scheduler(&scheduler);
        clk_gen->set_bus_controller(bc);
        cpu->set_scheduler(&scheduler);
    }
};
