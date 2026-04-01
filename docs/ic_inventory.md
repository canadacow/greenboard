# IC Inventory -- IBM 5150 64KB-256KB System Board

Source: `assets/pcb/64_256KB_SYSTEM_BOARD_rev1_2a.brd` (194 components, 320 nets)

## Implemented (30 IC types, 91 sockets)

| Ref | IC | Role |
|---|---|---|
| U3 | 8088 | CPU |
| U11 | 8284A | Clock generator |
| U6 | 8288 | Bus controller |
| U2 | 8259A | PIC |
| U34 | 8253-5 | PIT |
| U36 | 8255A | PPI |
| U35 | 8237A | DMA controller |
| U7,U9,U10,U18 | 74S373 | Address latches (ALE, DMA page) |
| U8,U12,U13,U14 | 74S245 | Bus transceivers (AD<->D, D<->MD, D<->XD, cmd strobes) |
| U15,U16,U17,U23 | 74S244 | Address/switch buffers. U23: SW1 DIP switch mux -> PPI Port A |
| U26,U98 | 74S175 | Quad D FFs. U26: PCLK divider + kbd sync. U98: AEN_BRD/HOLDA/RDY latches |
| U46,U47,U48,U65,U66 | 74S138 | Address decode (ROM/RAM/IO/ISA chip selects) |
| U64 | 74S20 | Dual 4-input NAND (ROM addr sel, wait state clock) |
| U62,U79 | 74S158 | DRAM address multiplexers (row/col select) |
| U81 | 74S00 | Quad NAND (RAS/CAS timing, virtual TD1 inside) |
| U52 | 74S00 | Quad NAND (HRQ gate, DCLK, I/O channel check) |
| U63 | 74S38/74S00 | Quad OC NAND (speaker, motor, SW2 sense). Uses 74S00 logic |
| U49,U97 | 74S08 | Quad AND (RAS gate, NMI, RDY_TO_DMA, AEN gate) |
| U51,U83,U99 | 74S04 | Hex inverters (~RESET_DRV, ~WE, ~HRQ, ~CLK88, T/C, etc.) |
| U5 | 74LS30 | 8-input NAND (bus idle detect) |
| U67,U82 | 74S74 | Dual D FFs. U67: HOLDA + DRQ0 latch. U82: keyboard IRQ1 + wait state generator |
| U19 | 74LS670 | DMA page register (4x4 register file) |
| U84 | 74S10 | Triple 3-input NAND (DACK/AEN gating, ~DEN) |
| U27 | 74LS02 | Quad NOR (ROM/RAM/IO select, transceiver enables) |
| U50 | 74S02 | Quad NOR (~DMA_AEN, ~WRT_NMI_REG, I/O decode) |
| U101 | 74LS32 | Quad OR (PIT ~RD/~WR, ~DMA_CS gating) |
| U37-U45,U53-U61,U69-U77,U85-U93 | 4164 (IC_DRAM_256K) | DRAM banks 0-3 (36 chips, 256KB + parity) |
| U29-U33 | IC_ROM_40K | ROM (5 sockets, single behavioral IC) |

## Not Implemented (9 sockets)

| Ref | IC | Pins | Role |
|---|---|---|---|
| U24 | 74S322 | 20 | Keyboard shift register. Serial KBD data in, parallel out to PPI Port A |
| U94 | 74S280 | 14 | Parity generator/checker. MD0-MD7 -> even/odd parity for DRAM 9th bit |
| U96 | 74S74 | 14 | Dual D FF. FF1: parity check latch. FF2: NMI enable gate |
| U80 | 74S125 | 14 | Quad tri-state buffer. SW2 sense, parity write, KBD reset/inhibit |
| U1 | MC1741 | 8 | Speaker op-amp |
| U95 | 75477 | 8 | Speaker driver (dual Darlington) |
| TD2 | TIME_DELAY_1 | 3 | DRAM CAS timing delay |
| U28 | 8K_X_8ROS | 24 | ROM socket (F6000-F7FFF, unpopulated in most configs) |
| U100 | 20DIP300 | 20 | Empty socket (not in BRD netlist) |

## Optional (not modeled)

| Ref | IC | Role |
|---|---|---|
| XU4 | 8087 | Math coprocessor socket (unpopulated) |

## DMA Subsystem

The 8237A (U35) and its supporting glue logic (U67, U98, U19, U50, U52, U62, U79, U49, U81, TD1) implement full 4-channel DMA. Channel 0 handles DRAM refresh (auto-init, single transfer from PIT CH1). Channels 1-3 are available for ISA peripherals.

## ISA Expansion Cards (5 implementations)

| Slot | Card | Ports | MMIO | DMA | IRQ | Description |
|---|---|---|---|---|---|---|
| J1 (slot 0) | ISA_TestCard | 0x80-0xFF | -- | CH1,CH3 | 2-7 | Test/debug card. HostFS (0xE0-0xEF), test control (0xF0-0xFD), kbd interface |
| J2 (slot 1) | ISA_FloppyController | 0x3F2-0x3F5 | -- | CH2 | 6 | NEC uPD765 FDC. DOR/MSR/FIFO. 2 drives, hot-swappable images |
| J3 (slot 2) | ISA_CGA | 0x3D0-0x3DF | B8000-BFFFF | -- | -- | CGA. 6845 CRTC + 16KB VRAM. GPU compute shader rendering, scanline stamping |
| J4 (slot 3) | ISA_RAM | -- | 40000-9FFFF | -- | -- | 384KB SRAM expansion (256KB motherboard -> 640KB total) |
| -- | ISA_MDA | 0x3B0-0x3BB | B0000-B0FFF | -- | -- | MDA. 80x25 text only. Swappable with CGA in slot 2 |

## Non-IC Components (all implemented)

- J1-J5: ISA slots (62p each, wired via IsaSlot + ISA_Bus)
- J7: Keyboard port (TestKeyboard)
- J6: Cassette port (not modeled -- unused by DOS)
- J8: +RUN jumper (not modeled)
- SW1: 8-position DIP switch (IC_DipSwitch, RAM/display/FPU config)
- SW2: 4-position DIP switch (IC_DipSwitch, RAM bank config)
