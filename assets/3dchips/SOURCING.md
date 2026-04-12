# Remaining Components to Source

## POWER_CON (P1, P2) -- PSU Power Connectors

The IBM 5150 uses two identical 6-pin Molex connectors for the PSU.
BRD footprint has 7 pads (6 power + 1 key/ground tab).

- **Connector type:** Molex 8981 series (AMP Mate-N-Lok equivalent)
- **Pin count:** 6 positions, single row
- **Pitch:** 0.200" (5.08 mm)
- **Gender:** Female (receptacle on motherboard, male on PSU cable)
- **Mounting:** Vertical through-hole, right-angle
- **Pin assignments (P1):** GND, GND, -5V, +5V, +5V, +5V
- **Pin assignments (P2):** GND, GND, -12V, +12V, Power Good, +5V
- **Search terms:** "Molex 8981" OR "AMP Mate-N-Lok 6 pin" OR "AT PSU motherboard connector"
- **Modern equivalent:** Molex 09-65-2068 (or any 6-pin 0.2" Molex disk connector)

## SW1 (8-position DIP switch) and SW2 (4-position DIP switch)

These are actual DIP switches, not ICs. They have toggle levers on top.

- **SW1 ref:** U-position, footprint DIP-16__300 (8 switches = 16 pins)
- **SW2 ref:** U-position, footprint DIP-16__300 (but only 4 switches, still 16-pin body? Verify -- might be 8-pin)
- **Body:** Standard 0.3" (7.62 mm) row spacing, same as DIP-16
- **Search terms:** "DIP switch 8 position STEP" OR "CTS 206-8" OR "Grayhill 76SB08"
- **Note:** SW1 configures RAM size, display type, FPU presence. SW2 configures RAM bank count.

## RN1-RN4 (Resistor Networks)

SIP or DIP resistor arrays. BRD footprint is DIP-16__300.

- **Package:** 16-pin DIP, 0.3" row spacing
- **Type:** Bussed or isolated resistor network
- **Search terms:** "resistor network DIP-16 STEP" OR "Bourns 4116R" OR "CTS 766"
- **Note:** Visually identical to a DIP IC but typically has a different top marking (dot array or value code). For 3D purposes the body is the same as DIP-16.

## VR / VC1 (Variable Capacitor / Trimmer)

- **BRD footprint:** VR, 3 pads
- **Value:** 5-30 pF
- **Type:** Ceramic trimmer capacitor (not a potentiometer despite "VR" name)
- **Package:** Through-hole, small rectangular or cylindrical body
- **Search terms:** "ceramic trimmer capacitor 5-30pF STEP" OR "Murata TZB4" OR "Sprague-Goodman GKG"
- **Typical dimensions:** ~7 x 7 x 4 mm
- **Note:** Used for fine-tuning the 14.31818 MHz crystal oscillator frequency

## D1 (Diode)

- **BRD footprint:** D5, 2 pads
- **Value:** TYPE_FC
- **Type:** Axial through-hole diode (likely 1N4148 or similar signal diode)
- **Package:** DO-35 glass body (small axial, ~3.5 mm long, 1.5 mm dia)
- **Search terms:** "diode DO-35 STEP" OR "1N4148 3D model"
- **KiCad check:** `Diode_THT.3dshapes/` -- likely has DO-35

## TD1 / PE-21712 (Pulse Engineering Delay Line)

- **BRD footprint:** PE-21712, 8 pads
- **Value:** TIME_DELAY_2
- **Type:** Tapped delay line in DIP-8 package
- **Manufacturer:** Pulse Engineering (now Pulse Electronics)
- **Package:** DIP-8, 0.3" row spacing -- physically identical to a DIP-8 IC
- **Search terms:** "Pulse Engineering PE-21712" OR "delay line DIP-8 STEP"
- **Note:** Body is mechanically identical to DIP-8. Visual difference is the label only.

## TD2 (CAS Timing Delay)

- **BRD footprint:** TD2, 3 pads
- **Value:** TIME_DELAY_1
- **Type:** Simple RC delay or tapped delay element
- **Package:** 3-pin SIP, small package
- **Search terms:** "3 pin delay line STEP" OR "RC delay module SIP-3"
- **Note:** This is a tiny component. May need parametric modeling.

## K1 (DIP Relay)

- **BRD footprint:** G5V-2DPDT, 8 pads
- **Value:** DIP_RELAY
- **Type:** Omron G5V-2 DPDT signal relay in DIP package
- **Package:** DIP-8 outline but TALLER than a standard DIP IC (~10 mm vs ~3.5 mm)
- **Manufacturer:** Omron
- **Search terms:** "Omron G5V-2 STEP" OR "DIP relay G5V-2 3D model"
- **KiCad check:** `Relay_THT.3dshapes/` -- may have G5V-2
- **Note:** The G5V-2 has a distinctive tall rectangular body. Using a standard DIP-8 would be the wrong height.

## HOLE (H1-H9, Mounting Holes)

- 9 mounting holes. No 3D model needed -- just through-holes in the PCB mesh.
