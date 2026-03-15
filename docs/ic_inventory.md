# IC Inventory -- IBM 5150 64KB-256KB System Board

Source: `assets/pcb/64_256KB_SYSTEM_BOARD_rev1_2a.brd` (194 components, 320 nets)

## Implemented (20 IC types, 63 sockets)

| Ref | IC | Role |
|---|---|---|
| U3 | 8088 | CPU |
| U11 | 8284A | Clock generator |
| U6 | 8288 | Bus controller |
| U2 | 8259A | PIC |
| U34 | 8253-5 | PIT |
| U36 | 8255A | PPI |
| U35 | 8237A | DMA |
| U7,U9,U10 | 74S373 | Address latches |
| U8 | 74S245 | Data bus transceiver (AD<->D) |
| U12 | 74S245 | Memory data bus transceiver (D<->MD, DRAM read/write) |
| U26 | 74S175 | PCLK divider / kbd sync |
| U46,U47,U48,U65,U66 | 74S138 | Address decode (ROM/RAM/IO/ISA chip selects) |
| U64 | 74S20 | Dual 4-input NAND (ROM address decode) |
| U62,U79 | 74S158 | DRAM address multiplexers (row/col select) |
| U81 | 74S00 | Quad NAND (RAS/CAS timing, virtual TD1 inside) |
| U49 | 74S08 | Quad AND (RAS decode gating) |
| U83 | 74S04 | Hex inverter (~WE buffer for DRAM) |
| U37-U45,U53-U61,U69-U77,U85-U93 | 4164 (IC_DRAM_256K) | DRAM banks 0-3 (36 chips, 256KB + parity) |
| U28-U33 | 8K_X_8ROS | ROM (6 sockets) |

## Not Implemented -- Meaningful ICs (11 sockets)

| Ref | IC | Pins | Role |
|---|---|---|---|
| U13,U14 | 74S245 | 20 | System bus transceivers (D<->XD, cmd strobes) |
| U15,U16,U17 | 74S244 | 20 | Address bus buffers (A0-A19 onto system bus) |
| U23 | 74S244 | 20 | Data bus buffer (ISA slots) |
| U18 | 74S373 | 20 | DMA page latch |
| U19 | L670 (74LS670) | 16 | DMA page register (4x4 register file) |
| U24 | 74S322 | 20 | Keyboard shift register (serial->parallel) |
| U98 | 74S175 | 16 | Quad D FF (second instance, kbd/timing) |
| U94 | 74S280 | 14 | Parity generator/checker |
| U67,U82,U96 | 74S74 | 14 | D flip-flops (DMA req, wait state, NMI gate) |
| U1 | MC1741 | 8 | Speaker op-amp |
| U95 | 75477 | 8 | Speaker driver |
| TD2 | TIME_DELAY_1 | 3 | DRAM timing delay |
| U100 | 20DIP300 | 20 | Empty socket (unpopulated) |
| XU4 | 8087 socket | 40 | Math coprocessor (optional) |

## Not Implemented -- Glue Logic (9 sockets)

| Ref | IC | Role |
|---|---|---|
| U5 | 74LS30 | 8-input NAND (composite READY) |
| U27 | 74LS02 | Quad NOR |
| U97 | 74S08 | Quad AND |
| U50 | 74S02 | Quad NOR |
| U51,U99 | 74S04 | Hex inverter |
| U52 | 74S00 | Quad NAND |
| U63 | 74S38 | Quad OC NAND |
| U80 | 74S125 | Quad tri-state buffer |
| U84 | 74S10 | Triple 3-input NAND |
| U101 | 74LS32 | Quad OR |

## Non-IC Components

- J1-J5: ISA slots (62p each)
- J6: Cassette port, J7: Keyboard port, J8: +RUN jumper
- SW1, SW2: DIP switch banks (config: RAM size, display, FPU)

## Bus Timing (8088 + 8288 + 74S373 + 74S245)

Half-cycle evaluation has been removed -- the scheduler evaluates all components once per full CLK cycle.
The 8288 drops ALE and asserts command strobes (~MEMR/~MEMW/~IOR/~IOW/~INTA) and ~DEN at the T1->T2 transition.
BusGlue reads latched address (XA) and drives/reads data based on T-state progression.
