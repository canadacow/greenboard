# 3D Parts Mapping -- IBM 5150 64KB-256KB System Board

Maps BRD footprint names to 3D model files for Blender rendering.
Dimensions from Intel ISA Bus Spec and TE Connectivity Catalog 1654080.

## Source: KiCad packages3D (gitlab.com/kicad/libraries/kicad-packages3D)

| BRD Footprint | Pins | 3D File | KiCad Source Path | Sockets |
|---|---|---|---|---|
| DIP-8__300 | 8 | DIP-8_W7.62mm.step | Package_DIP.3dshapes/ | U1, U95 |
| DIP-14__300 | 14 | DIP-14_W7.62mm.step | Package_DIP.3dshapes/ | U5, U27, U49-U52, U63-U64, U67, U80-U84, U94, U96-U97, U99, U101 |
| DIP-16__300 | 16 | DIP-16_W7.62mm.step | Package_DIP.3dshapes/ | U26, U37-U45, U46-U48, U53-U61, U62, U65-U66, U69-U77, U79, U85-U93, U98, U19 |
| DIP-18__300 | 18 | DIP-18_W7.62mm.step | Package_DIP.3dshapes/ | U11 |
| DIP-20__300 | 20 | DIP-20_W7.62mm.step | Package_DIP.3dshapes/ | U6-U10, U12-U18, U23-U24, U100 |
| DIP-24__600 | 24 | DIP-24_W15.24mm.step | Package_DIP.3dshapes/ | U28-U34 |
| DIP-28__600 | 28 | DIP-28_W15.24mm.step | Package_DIP.3dshapes/ | U2 |
| DIP-40__600 | 40 | DIP-40_W15.24mm.step | Package_DIP.3dshapes/ | U3, U35-U36, XU4 |

### DIP Socket variants (for socketed ICs)

| BRD Footprint | 3D File | KiCad Source Path | Use for |
|---|---|---|---|
| DIP-24__600 | DIP-24_W15.24mm_Socket.step | Package_DIP.3dshapes/ | ROM sockets U28-U33 |
| DIP-40__600 | DIP-40_W15.24mm_Socket.step | Package_DIP.3dshapes/ | CPU socket U3, FPU socket XU4 |
| DIP-16__300 | DIP-16_W7.62mm_Socket.step | Package_DIP.3dshapes/ | DRAM sockets U37-U45, U53-U61, U69-U77, U85-U93 |

### Resistor networks (DIP-16 body but labeled as RN)

| Ref | Value | Same footprint as |
|---|---|---|
| RN1-RN4 | Resistor network | DIP-16__300 |

### DIP switches (DIP-16 body)

| Ref | Value | Same footprint as |
|---|---|---|
| SW1 | 8-pos DIP switch | DIP-16__300 |
| SW2 | 4-pos DIP switch | DIP-16__300 |

## Source: TE Connectivity / VOGONS (7-5530843-0)

| BRD Footprint | Pins | 3D File | Notes |
|---|---|---|---|
| 62 | 62 | 7-5530843-0.step | ISA 8-bit slot. J2-J5 |
| 62PinEdgeIOConnector | 62 | 7-5530843-0.step | ISA 8-bit slot. J1 (same part, different footprint name in BRD) |

## Source: 3D ContentCentral (5-pin DIN)

| BRD Footprint | Pins | 3D File | Notes |
|---|---|---|---|
| 5PINDIN | 8 | User Library-DIN-5.STEP | Cassette port J6. 18 solids (housing/insulator/contacts pre-split) |
| 5PINDIN2 | 8 | User Library-DIN-5.STEP | Keyboard port J7. Same connector |

## Source: KiCad packages3D (passives, headers, crystal)

| BRD Footprint | Pins | 3D File | KiCad Source Path | Refs |
|---|---|---|---|---|
| R5 | 2 | R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal.step | Resistor_THT.3dshapes/ | R1-R25 (24 resistors) |
| C1-1 | 3 | C_Disc_D3.0mm_W1.6mm_P2.50mm.step | Capacitor_THT.3dshapes/ | C1-C48 (~39 ceramic disc caps) |
| CP8 | 2 | C_Disc_D7.5mm_W2.5mm_P5.00mm.step | Capacitor_THT.3dshapes/ | C5, C8, C9 (3 caps) |
| VC | 4 | Crystal_HC49-U_Vertical.step | Crystal.3dshapes/ | Y1 (14.31818 MHz) |
| PIN_ARRAY_2X1 | 2 | PinHeader_1x02_P2.54mm_Vertical.step | Connector_PinHeader_2.54mm.3dshapes/ | J8 (+RUN jumper) |
| PIN_ARRAY_2X2 | 4 | PinHeader_2x02_P2.54mm_Vertical.step | Connector_PinHeader_2.54mm.3dshapes/ | P4 (Berg strip) |
| PIN_ARRAY_4x1 | 4 | PinHeader_1x04_P2.54mm_Vertical.step | Connector_PinHeader_2.54mm.3dshapes/ | P3 (4-pin header) |

## Source: KiCad packages3D (diode, relay)

| BRD Footprint | Pins | 3D File | KiCad Source Path | Refs |
|---|---|---|---|---|
| D5 | 2 | D_DO-35_SOD27_P10.16mm_Horizontal.step | Diode_THT.3dshapes/ | D1 (signal diode) |
| G5V-2DPDT | 8 | Relay_DPDT_Omron_G5V-2.step | Relay_THT.3dshapes/ | K1 (DIP relay) |
| VR | 3 | C_Trimmer_Murata_TZB4-B.step | Capacitor_SMD.3dshapes/ | VC1 (5-30pF trimmer) |
| POWER_CON | 7 | User Library-6 way 0_1inch pitch molex header.step | 3D ContentCentral | P1, P2 (PSU power) |
| PE-21712 | 8 | TD1_PE21712_delay.step | Parametric (FreeCAD) | TD1 (tapped delay line, 4-pin module) |
| TD2 | 3 | TD2_SIP3_delay.step | Parametric (FreeCAD) | TD2 (SIP-3 delay line, green ceramic) |

## Source: CTS (DIP switches)

| BRD Footprint | Pins | 3D File | Notes |
|---|---|---|---|
| DIP-16__300 (SW1) | 16 | 206-8.step | 8-position DIP switch. 10 solids (housing + toggles) |
| DIP-16__300 (SW2) | 16 | 206-8.step | 8-position DIP switch (same part as SW1) |

## Source: Bourns (resistor networks)

| BRD Footprint | Pins | 3D File | Notes |
|---|---|---|---|
| DIP-16__300 (RN1-RN4) | 16 | BO_4116R.step | Bourns 4116R. 18 solids (body + dot + 16 leads) |

## Not yet sourced

| BRD Footprint | Pins | Refs | Value | Notes |
|---|---|---|---|---|

## Files present in this directory

```
assets/3dchips/
  7-5530843-0.step           -- ISA 8-bit slot (TE Connectivity)
  C_Disc_D3.0mm_W1.6mm_P2.50mm.step    -- ceramic disc cap (KiCad)
  C_Disc_D7.5mm_W2.5mm_P5.00mm.step    -- larger disc cap (KiCad)
  Crystal_HC49-U_Vertical.step          -- crystal oscillator (KiCad)
  DIP-8_W7.62mm.step         -- 8-pin 300-mil DIP (KiCad)
  DIP-14_W7.62mm.step        -- 14-pin 300-mil DIP (KiCad)
  DIP-16_W7.62mm.step        -- 16-pin 300-mil DIP (KiCad)
  DIP-16_W7.62mm_Socket.step -- 16-pin 300-mil DIP socket (KiCad)
  DIP-18_W7.62mm.step        -- 18-pin 300-mil DIP (KiCad)
  DIP-20_W7.62mm.step        -- 20-pin 300-mil DIP (KiCad)
  DIP-24_W15.24mm.step       -- 24-pin 600-mil DIP (KiCad)
  DIP-24_W15.24mm_Socket.step-- 24-pin 600-mil DIP socket (KiCad)
  DIP-28_W15.24mm.step       -- 28-pin 600-mil DIP (KiCad)
  DIP-40_W15.24mm.step       -- 40-pin 600-mil DIP (KiCad)
  DIP-40_W15.24mm_Socket.step-- 40-pin 600-mil DIP socket (KiCad)
  PinHeader_1x02_P2.54mm_Vertical.step  -- 1x2 pin header (KiCad)
  PinHeader_1x04_P2.54mm_Vertical.step  -- 1x4 pin header (KiCad)
  PinHeader_2x02_P2.54mm_Vertical.step  -- 2x2 pin header (KiCad)
  D_DO-35_SOD27_P10.16mm_Horizontal.step  -- signal diode (KiCad)
  R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal.step -- resistor (KiCad)
  Relay_DPDT_Omron_G5V-2.step             -- DIP relay (KiCad)
  206-8.step                              -- 8-pos DIP switch (CTS)
  BO_4116R.step                           -- DIP-16 resistor network (Bourns)
  C_Trimmer_Murata_TZB4-B.step           -- ceramic trimmer cap (KiCad)
  User Library-6 way 0_1inch pitch molex header.step -- Molex PSU connector (3D ContentCentral)
  User Library-DIN-5.STEP    -- 5-pin DIN connector (3D ContentCentral)
  PARTS.md                   -- this file
  SOURCING.md                -- details for remaining unsourced parts
```
