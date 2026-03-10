# Bench — IBM PC 5150 Motherboard Emulator Architecture

## Core Principle

Emulate the motherboard at the **interconnect level**. ICs are behaviorally emulated (black boxes), but every wire, trace, bus line, and signal between them is explicitly modeled. This includes passive components (resistors, capacitors, DIP switches, jumpers) and power rails.

## The Wire-as-Mailbox Model

- A **wire is a mailbox**. A signal change is a message. Every pin connected to that wire is a subscriber.
- Each **IC gets its own thread**. ICs react to signal changes on their input pins and drive signal changes on their output pins.
- The **clock is just another signal** — when CLK changes, every IC that cares about CLK wakes up and reacts.
- This directly mirrors real hardware: wires carry signals, ICs react concurrently.

## What Gets Modeled

Everything on the physical motherboard:
- **ICs**: 8088 CPU, 8284 clock gen, 8288 bus controller, 8259 PIC, 8253 PIT, 8237 DMA, 8255 PPI, 74LS373 latches, 74LS245 transceivers, ROM/RAM chips
- **Passive components**: resistors, capacitors, DIP switches (SW1/SW2), jumpers
- **Power**: +5V, +12V, -5V, -12V rails from PSU — modeled as enable signals, no power = nothing ticks
- **Connectors**: ISA slots (62-pin edge connectors), keyboard DIN, cassette, speaker
- **All signal lines**: address bus (A0-A19), data bus (D0-D7), control signals (~MEMR, ~MEMW, ~IOR, ~IOW, ALE, etc.), IRQ lines, DMA lines

## BRD-Driven Wiring

All motherboard wiring is sourced at runtime from the KiCad legacy BRD file (`assets/pcb/64_256KB_SYSTEM_BOARD_rev1_2a.brd`). No manual wiring code exists.

- **`BrdParser`** (`src/board/brd_parser.cpp`) parses the BRD file, extracting nets (`$EQUIPOT` sections) and component pin-to-net assignments (`$MODULE/$PAD` sections).
- **`Motherboard::wire_from_brd()`** iterates all 194 parsed components and wires each pin to its net's `Signal*`.
- **Net resolution**: Named nets (e.g. `CLK`, `A0`, `+5V`) are mapped to pre-declared `Signal` members via `build_net_map()` (~150 mappings). Anonymous nets (`N-000xxx`) get dynamically created `Signal` objects stored in `dynamic_signals_`.
- **Component registry**: `build_component_registry()` registers all board objects (sockets, ISA slots, DIP switches, passives, connectors) by ref designator for lookup during wiring.
- **Verification**: The `Multimeter::audit()` function re-parses the BRD and verifies that all pins on each net share the same `Signal*` pointer — a full continuity check (305 nets, 1571 pins, 0 failures).

## Signal/Wire Abstraction

Implemented in `src/core/signal.h`:
- `Signal` class: named wire with tri-state logic (Low, High, Hi-Z), observer pattern via `connect(Component*)` for mailbox delivery.
- `Bus` class: bundle of N signal lines (e.g., address bus = 20 signals).

## Threading Model

### Component Base Class (`src/core/component.h`)

Every IC extends `Component`, which provides:
- **`power_on()`** / **`power_off()`**: Start/stop the IC's `std::jthread`.
- **`run(std::stop_token)`**: Virtual. The IC's main loop. Default implementation is reactive (blocks on mailbox).
- **`post(SignalEvent)`**: Thread-safe mailbox enqueue + notify.
- **`wait_mailbox(stop)`**: Blocking wait for at least one event, then drains all pending.
- **`drain_mailbox()`**: Non-blocking — swaps the queue under lock, dispatches all events without holding the lock.
- **`on_signal_change(signal, old, new)`**: Virtual callback for processing individual signal events.
- **`stop_requested()`**: Check the thread's stop token.

### Reactive vs Active Components

Two patterns for IC threads:

**Reactive** (default `run()` implementation):
```
on_power_on();
while (!stop) wait_mailbox(stop);   // blocks until a signal arrives
on_power_off();
```
Used by ICs that only respond to input changes (e.g. latches, decoders).

**Active** (override `run()`):
```
wait for VCC High via wait_mailbox();   // sleep until powered
spin loop {
    toggle outputs (oscillator, counters, etc.)
    drain_mailbox();   // non-blocking: handle input changes
    check VCC still High; break if not
}
release all outputs;
```
Used by ICs with autonomous behavior (oscillators, timers). The 8284A clock generator is the canonical example.

### VCC-Driven Power

IC threads don't spin until the board is powered on. The pattern is:
1. IC thread starts (via `power_on()`) and blocks on `wait_mailbox()`.
2. `Motherboard::power_on()` drives VCC High.
3. VCC change is posted to the IC's mailbox, waking the thread.
4. IC enters its active loop.
5. `Motherboard::power_off()` drives VCC Low.
6. IC detects VCC drop on next loop iteration and exits cleanly.

## IC Install / Insert Pattern

ICs are plugged into sockets in two steps:

1. **`ic->install(socket)`**: The IC reads pin signals from the socket (via `socket.pin_signal(pin)`) and subscribes to input signals (via `signal->connect(this)`). This binds the IC to the board's wiring.
2. **`socket.insert(std::move(ic))`**: Transfers ownership of the IC to the socket.

This separation ensures the IC has its signal pointers set up before any events can arrive.

## Multimeter (`src/tools/multimeter.h`)

A virtual test instrument for verifying board wiring:
- **`probe(ref, pin)`**: Returns the `Signal*` at a component pin.
- **`continuity(ref1, pin1, ref2, pin2)`**: Checks two pins share the same `Signal*`.
- **`audit(brd_path)`**: Full-board continuity test against the BRD netlist.
- **`drive()` / `read()` / `release()`**: Inject/read signal levels for testing.

## 8284A Clock Generator (`src/ic/ic_8284a.h`)

First implemented IC. 18-pin DIP, generates the master clock.

- **OSC** (pin 12): 14.31818 MHz oscillator output — toggles every tick.
- **CLK** (pin 8): OSC / 3 = 4.77 MHz, 33% duty cycle (high 2 OSC half-periods, low 4).
- **PCLK** (pin 2): CLK / 2 = 2.38 MHz, 50% duty cycle.
- **RESET** (pin 10): Synchronized to CLK falling edge. Inverted RES input.
- **READY** (pin 5): Synchronized to CLK falling edge. RDY1 gated by ~AEN1.

Uses the **active component** pattern: spin loop as oscillator, `drain_mailbox()` each tick for RDY/RES/VCC input changes.

## Future

- 3D visualization of the motherboard (separate concern, will come later)
- The architecture should be visualization-friendly: named signals, named components, physical positions can be layered on later
