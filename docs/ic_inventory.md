# IC Inventory -- IBM 5150 64KB-256KB System Board

Source: `assets/pcb/64_256KB_SYSTEM_BOARD_rev1_2a.brd` (194 components, 320 nets)

## Implemented (26 IC types, 84 sockets)

| Ref | IC | Role |
|---|---|---|
| U3 | 8088 | CPU |
| U11 | 8284A | Clock generator |
| U6 | 8288 | Bus controller |
| U2 | 8259A | PIC |
| U34 | 8253-5 | PIT |
| U36 | 8255A | PPI |
| U35 | 8237A | DMA controller (S1-S4 state machine). Disableable -- see DMA note below |
| U7,U9,U10,U18 | 74S373 | Address latches. U7: AD0-7 -> A0-A7 (ALE latch, AEN_BRD enable). U9: A8-A15_BUS -> A8-A15 (ALE latch, AEN_BRD enable). U10: A16-A19_BUS -> A16-A19 (ALE latch, AEN_BRD enable). U18: XD0-7 -> A8-A15 (DMA high-byte latch, ~DMA_AEN enable, latched on N-000280/U35.8) |
| U8,U12,U13,U14 | 74S245 | Bus transceivers. U8: AD0-7<->D0-7 (DIR=DT/~R from U6.4, ~OE=N-000215 from U84.12). U12: D0-7<->MD0-7 (~OE=~XMEMR, DIR=~RAM_ADDR_SEL). U13: D0-7<->XD0-7 (~OE=N-000290/U27.1, DIR=AEN_BRD). U14: ~XMEMR/~XMEMW/~XIOW/~XIOR <-> ~MEMR/~MEMW/~IOW/~IOR (~OE=~DMA_AEN, cmd strobe buffer to ISA bus) |
| U15,U16,U17 | 74S244 | Address bus buffers. U16: A0-A7->XA0-XA7 (AEN_BRD enable). U17: A0-A7->XA0-XA3 + DMA high addr A4-A7 from U35.37-40 (~DMA_AEN enable). U15: A8-A12->XA8-XA12 + CLK88/~DACK0/AEN onto ISA bus |
| U26,U98 | 74S175 | Quad D FFs. U26: CLK from PIT OUT (N-000329). FF2=PCLK/2 divider (1.193MHz), FF0/FF1=keyboard data+clock sync (pin 4->J7.1/U80.11, pin 6->U24.11/U82.3). U98: CLK from system CLK. Latches AEN_BRD, HOLDA, ~RDY/WAIT, DMA wait signals |
| U46,U47,U48,U65,U66 | 74S138 | Address decode (ROM/RAM/IO/ISA chip selects) |
| U64 | 74S20 | Dual 4-input NAND. Gate1: A16-A19 -> ~ROM_ADDR_SEL (enables U46 ROM decode). Gate2: ~XIOR & ~XIOW & N-000239(x2 from U84.6) -> N-000240 (clocks U82 FF2 wait state generator) |
| U62,U79 | 74S158 | DRAM address multiplexers (row/col select) |
| U81 | 74S00 | Quad NAND (RAS/CAS timing, virtual TD1 inside). Gate1: DACK0 & RAS -> ~REFRSH_GATE. Gate2: ~XMEMR & ~XMEMW -> RAS (memory access triggers RAS). Gate3: TD1.8 & TD1.12 -> ~CAS (TD1 delay outputs generate CAS). Gate4: N.P._INSTL_SW & N.P._NPI -> N-000236 (install/coprocessor detect) |
| U52 | 74S00 | Quad NAND. Gate1: RN1.12 & ~HRQ_DMA -> N-000242 (HRQ gating -> U5/U67). Gate2: N-000247(~CLK88) & N-000249(TD2 out) -> DCLK (U35.12). Gate3: ~CLK & I/O_CH_CK -> N-000234. Gate4: N-000234(x2) -> ~I/O_CH_CK (ISA bus) |
| U49,U97 | 74S08 | Quad AND. U49: gates U65 outputs (RAS bank selects) with ~REFRSH_GATE -> ~RAS0-~RAS3 (DRAM bank enables). U97: Gate1: N-000235 & U96.Q2(NMI enable) -> NMI. Gate2: MDP & N-000306(~XMEMR inv) -> parity check. Gate3: RDY_TO_DMA -> U35.6. Gate4: N-000244 & AEN_BRD -> N-000246(~DMA_AEN input) |
| U51,U83,U99 | 74S04 | Hex inverters. U51: RESET->~RESET_DRV (3 inverters), CLK->~CLK (for TD2), ~ENABLE_I/O_CK inversion, ~WRT_DMA_PG_REG strobe. U83: ~XMEMW->~WE (via RN4), ~XMEMR inversions (2), bus idle inversion, ~DACK_0_BRD->DACK0. U99: HRQ->~HRQ_DMA, ~EOP->T/C (ISA bus), ~ENB_RAM_PCK, ~CLK88, speaker signal, KBD/SW1 mux (N-000371->N-000365) |
| U5 | 74LS30 | 8-input NAND (bus idle detect) |
| U67 | 74S74 | Dual D FF (HOLDA, DRQ0 latch) |
| U19 | 74LS670 | DMA page register (4x4 register file) |
| U84 | 74S10 | Triple 3-input NAND. Gate A: N-000220(x2, U2.16 CAS addr) & N-000214(U6.16) -> N-000215 (U8 ~OE, ~DEN for AD<->D transceiver). Gate B: ~DACK_0_BRD & AEN_BRD & N-000241(U83.10) -> N-000239 (U64 gate2 input). Gate C: N-000236(U81.11) & ~PCK(U96.4/6) & N-000234(U52.8/12) -> N-000235 (U97.1, parity NMI gate) |
| U27 | 74LS02 | Quad NOR. Gate1: N-000303 NOR N-000288 -> N-000290 (U13 ~OE, D<->XD transceiver enable). Gate2: ~ROM_ADDR_SEL NOR ~XMEMR -> N-000288 (ROM access detect). Gate3: ~RAM_ADDR_SEL NOR U94.6(parity) -> N-000317 (U96 FF1 D input). Gate4: XA9 NOR ~XIOR -> N-000303 (high I/O range decode) |
| U50 | 74S02 | Quad NOR. Gate1: N-000246(x2) -> ~DMA_AEN. Gate2: N-000262(x2) -> ~WRT_NMI_REG (U96.11). Gate3: U66.10 NOR ~XIOW -> N-000262 (I/O decode). Gate4: ~XIOW NOR U66.11 -> N-000261 (I/O decode -> U51.13) |
| U101 | 74LS32 | Quad OR. Gate1: ~XIOW OR U34.21 -> U34.23 (~WR for PIT). Gate2: ~XIOR OR U34.21 -> U34.22 (~RD for PIT). Gate4: ~DMA_CS(U66.15) OR ~XIOW -> U5.3 (bus idle input). Gate3 unused |
| U37-U45,U53-U61,U69-U77,U85-U93 | 4164 (IC_DRAM_256K) | DRAM banks 0-3 (36 chips, 256KB + parity) |
| U28-U33 | 8K_X_8ROS | ROM (6 sockets) |

## Not Implemented -- Meaningful ICs (7 sockets)

| Ref | IC | Pins | Role |
|---|---|---|---|
| U23 | 74S244 | 20 | SW1 DIP switch buffer -> 8255A Port A + 74S322 keyboard. Tri-state enables (N-000371 via U82/U99) mux Port A between switch config and keyboard scancodes |
| U24 | 74S322 | 20 | Keyboard shift register. Serial KBD data in (J7.2 via U80.8), parallel out to U36 Port A (pins 4-7, 13-16). ~OE on N-000371 (shared with U23 enables -- mutual exclusion). QH (pin 12) -> U82.2, IRQ1 (pin 1) -> U2.19 + U82.5 |
| U94 | 74S280 | 14 | Parity generator/checker. Inputs: MD0-MD7 (8 data bits from DRAM bus). Output: even/odd parity (pin 5 -> U80.5, pin 4 -> U97.6). Pin 6 -> U27.9 (RAM select decode). Checks/generates 9th bit (MDP) for DRAM parity |
| U82 | 74S74 | 14 | Dual D flip-flop. FF1 (pins 1-6): keyboard/SW1 mux -- ~CLR (pin 1) = N-000371 (U23/U24 ~OE select), D (pin 2) from U24.12, CLK (pin 3) from U24.11/U26.6, ~PRE (pin 4) = N-000346 (U80.10), Q (pin 5) = IRQ1 -> PIC, ~Q (pin 6) = N-000346 (feeds back to ~PRE, self-hold latch). FF2 (pins 8-13): wait state generator -- ~Q (pin 8) -> U97.9, Q (pin 9) = ~RDY/WAIT -> U11.3 (8284A READY) + U98.12, ~PRE (pin 10) = I/O_CH_RDY (ISA bus, active-low preset triggers wait states), CLK (pin 11) from U64.8, D (pin 12) = ~DACK_0_BRD, ~CLR (pin 13) = N-000244 |
| U96 | 74S74 | 14 | Dual D flip-flop. FF1 (pins 1-6): parity check latch -- ~CLR (pin 1) = N-000308 -> U99.6, D (pin 2) from U27.10 (RAM parity status), CLK (pin 3) = ~XMEMR, ~PRE (pin 4) = ~PCK (U84.10), Q (pin 5) = PCK -> U36.10 (PPI), ~Q (pin 6) = ~PCK -> U84.10 (~Q feeds ~PRE, self-hold). FF2 (pins 8-13): NMI gate -- Q (pin 9) = NMI enable -> U97.2 (ANDed with parity to generate NMI), ~PRE (pin 10) = +5V (inactive), CLK (pin 11) = ~WRT_NMI_REG (U50.4), D (pin 12) = XD7, ~CLR (pin 13) = ~RESET_DRV |
| U1 | MC1741 | 8 | Speaker op-amp. Amplifies filtered speaker signal (pin 2 via R2/R3, pin 3 via R4/R5) to drive U95. Powered from -5V (pin 4), +5V (pin 7). Output (pin 6) through R1/R2 feedback |
| U95 | 75477 | 8 | Speaker driver. Dual Darlington. Ch1 (pins 2,3): drives speaker K1.16 from U63.6/RN3.10. Ch2 (pins 6,7): cassette relay -- input from U99.10, output through C9/R10 |
| TD2 | TIME_DELAY_1 | 3 | DRAM timing delay. Input (pin 1) from U51.8 (~CAS inverted). Output (pin 4) -> R11/R12 + U52.5 (DCLK generation). Adds CAS-to-data propagation delay for DRAM read timing |
| U100 | 20DIP300 | 20 | Empty socket (not in BRD netlist -- unpopulated) |
| XU4 | 8087 socket | 40 | Math coprocessor. Shares AD0-AD7, A8-A19, CLK88, RESET, READY, ~S0-~S2 with U3 (8088). ~RQ/~GT (pin 31) for bus arbitration. Pin 32 (NPI) -> U81.13 |

## Not Implemented -- Glue Logic (2 sockets)

| Ref | IC | Role |
|---|---|---|
| U63 | 74S38 | Quad OC NAND. Gate1 (pins 1,2,3): ANDs T/C_2_OUT (PIT CH2) with itself -> R8/R9 (speaker filter). Gate2 (pins 4,5,6): ANDs MOTOR_OFF (PPI PB3) -> U95.2 (speaker driver enable). Gate3 (pins 8,9,10): SW2 sense (pins 13-16) with PPI PB4 (U36.20/U80.1). Gate4 (pins 11,12,13): SPKR_DATA (PPI PB1) AND T/C_2_OUT -> RN3.14/U99.11 (speaker mixing) |
| U80 | 74S125 | Quad tri-state buffer. Buf1 (~OE=PPI PB4/U36.20, A=GND, Y=SW2.12): grounds SW2 sense line when PB4 active (SW2 readback enable). Buf2 (~OE=~WE, A=U94.5/parity out, Y=MDP): drives parity bit onto DRAM parity bus during writes. Buf3 (~OE=N-000346/U82 FF1, A=GND, Y=J7.2/KBD data): pulls keyboard data LOW for reset/inhibit. Buf4 (~OE=N-000347/PPI PC4, A=GND, Y=J7.1/KBD clock): pulls keyboard clock LOW for inhibit |

## DMA Subsystem

The 8237A (U35) and its supporting glue logic (U67, U98, U19, U50, U52, U62, U79, U49, U81, TD1) implement full 4-channel DMA. Channel 0 handles DRAM refresh (auto-init, single transfer from PIT CH1). Channels 1-3 are available for ISA peripherals.

DMA transfers require careful DAG cycle management. The 8237A's outputs (address, DACKs, HRQ, ~EOP, ~MEMR/~MEMW) feed back through the decode chain to its own inputs (~DMA_CS, CEN). These cycles are broken with bidir blocks that return HiZ for signals that are stable during a given phase -- e.g. HRQ/~EOP are HiZ during active transfers (S1-S4) since they don't change combinationally.

The 8237A uses a deferred write/read pattern: when ~IOW+~CS both go Low, a pending flag is set. The actual register write executes on the next evaluation, when bus data has propagated through the transceiver chain to the XD bus.

## Non-IC Components

- J1-J5: ISA slots (62p each, wired in test bench -- direct to XA/D/cmd, no buffer ICs)
  - J1: ISA_TestCard (I/O 0x80-0xFF, IRQ trigger 0xF0/0xF1, DMA ch1 data source 0xF4)
- J6: Cassette port, J7: Keyboard port, J8: +RUN jumper
- SW1, SW2: DIP switch banks (config: RAM size, display, FPU)

## Bus Timing (8088 + 8288 + 74S373 + 74S245)

Half-cycle evaluation has been removed -- the scheduler evaluates all components once per full CLK cycle.
The 8288 drops ALE and asserts command strobes (~MEMR/~MEMW/~IOR/~IOW/~INTA) and ~DEN at the T1->T2 transition.
ISA_TestCard in J1 decodes ports 0x80-0xFF and drives/reads SD0-7 on ~IOR/~IOW edges.
