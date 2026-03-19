# IC Inventory -- IBM 5150 64KB-256KB System Board

Source: `assets/pcb/64_256KB_SYSTEM_BOARD_rev1_2a.brd` (194 components, 320 nets)

## Implemented (30 IC types, 85 sockets)

| Ref | IC | Role |
|---|---|---|
| U3 | 8088 | CPU |
| U11 | 8284A | Clock generator |
| U6 | 8288 | Bus controller |
| U2 | 8259A | PIC |
| U34 | 8253-5 | PIT |
| U36 | 8255A | PPI |
| U35 | 8237A | DMA controller (S1-S4 state machine). Disableable -- see DMA note below |
| U7,U9,U10,U18 | 74S373 | Address latches + DMA page latch |
| U8,U12,U13,U14 | 74S245 | Bus transceivers (AD<->D, D<->MD, D<->XD, cmd strobes) |
| U15,U16,U17 | 74S244 | Address bus buffers (A0-A19 onto system bus) |
| U26,U98 | 74S175 | Quad D FFs (PCLK divider, AEN_BRD/DMA wait) |
| U46,U47,U48,U65,U66 | 74S138 | Address decode (ROM/RAM/IO/ISA chip selects) |
| U64 | 74S20 | Dual 4-input NAND (ROM address decode) |
| U62,U79 | 74S158 | DRAM address multiplexers (row/col select) |
| U81 | 74S00 | Quad NAND (RAS/CAS timing, virtual TD1 inside) |
| U52 | 74S00 | Quad NAND (HRQ gate, DCLK) |
| U49,U97 | 74S08 | Quad AND (RAS decode gating, RDY_TO_DMA) |
| U51,U83,U99 | 74S04 | Hex inverters (~RESET, ~WE, ~HRQ/~CLK88/~EOP) |
| U5 | 74LS30 | 8-input NAND (bus idle detect) |
| U67 | 74S74 | Dual D FF (HOLDA, DRQ0 latch) |
| U19 | 74LS670 | DMA page register (4x4 register file) |
| U84 | 74S10 | Triple 3-input NAND (DACK/AEN gating) |
| U27 | 74LS02 | Quad NOR (ROM/RAM select decode) |
| U50 | 74S02 | Quad NOR (~DMA_AEN, DMA wait state logic) |
| U101 | 74LS32 | Quad OR (~DMA_CS generation) |
| U37-U45,U53-U61,U69-U77,U85-U93 | 4164 (IC_DRAM_256K) | DRAM banks 0-3 (36 chips, 256KB + parity) |
| U28-U33 | 8K_X_8ROS | ROM (6 sockets) |

## Not Implemented -- Meaningful ICs (7 sockets)

| Ref | IC | Pins | Role |
|---|---|---|---|
| U23 | 74S244 | 20 | Data bus buffer (ISA slots) |
| U24 | 74S322 | 20 | Keyboard shift register (serial->parallel) |
| U94 | 74S280 | 14 | Parity generator/checker |
| U82,U96 | 74S74 | 14 | D flip-flops (wait state, NMI gate) |
| U1 | MC1741 | 8 | Speaker op-amp |
| U95 | 75477 | 8 | Speaker driver |
| TD2 | TIME_DELAY_1 | 3 | DRAM timing delay |
| U100 | 20DIP300 | 20 | Empty socket (unpopulated) |
| XU4 | 8087 socket | 40 | Math coprocessor (optional) |

## Not Implemented -- Glue Logic (2 sockets)

| Ref | IC | Role |
|---|---|---|
| U63 | 74S38 | Quad OC NAND |
| U80 | 74S125 | Quad tri-state buffer |

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
