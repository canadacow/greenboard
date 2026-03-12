# Bench -- IBM PC 5150 Motherboard Emulator Architecture

## Core Principle

Emulate the motherboard at the **interconnect level**. ICs are behaviorally emulated (black boxes), but every wire, trace, bus line, and signal between them is explicitly modeled. This includes passive components (resistors, capacitors, DIP switches, jumpers) and power rails.

## Execution Model: Fibers + One Thread

The simulation runs on a single OS thread. The 8284A clock generator owns that thread (via `std::jthread`) and acts as the crystal oscillator -- its spin loop IS the clock. All other ICs run as **Windows Fibers** on the 8284A's thread, cooperatively scheduled by the `Scheduler`.

- The 8284A toggles OSC, divides to CLK/PCLK. On each CLK edge it calls `scheduler->evaluate(self)`.
- `evaluate()` commits all pending signal changes, then resumes every registered fiber in order.
- Each fiber runs until it `yield()`s, then control returns to the scheduler which resumes the next fiber.
- Completely deterministic, single-threaded, zero synchronization overhead.

### Fiber Abstraction (`src/host_platform/fiber.h`)

Platform-agnostic C-like API. Windows headers are confined to `fiber_win32.cpp`.

```
Fiber fiber_convert_thread();           // convert calling thread to a fiber
void  fiber_revert_thread(Fiber f);     // revert back to a plain thread
Fiber fiber_create(size, entry, data);  // create a new fiber
void  fiber_delete(Fiber f);            // delete a fiber
void  fiber_switch(Fiber target);       // switch to target fiber
```

### Component Hierarchy

```
Component (abstract base)
  +-- ThreadedComponent    -- 8284A only (owns an OS thread)
  +-- FiberComponent       -- all other ICs (cooperative fibers)
```

**FiberComponent** (`src/core/fiber_component.h`):
- `power_on()`: Creates a fiber. First `resume()` enters `run()`.
- `power_off()`: Calls `on_power_off()`, deletes the fiber.
- `resume(caller)`: Switches to the fiber. The fiber runs until it calls `yield()`.
- Default `run()`: Calls `on_power_on()`, then loops `yield(); on_signal_change();`.
- Active ICs (8088) override `run()` with their own execution loop.

### Scheduler (`src/core/scheduler.h`)

Lightweight, non-owning. Called by the 8284A at each CLK edge.

```
evaluate(Fiber caller):
  1. Commit all dirty signals (pending -> level)
  2. Resume every registered fiber component in order
```

### Double-Buffered Signals

Signals are double-buffered to prevent mid-cycle glitches:
- `drive(lvl)` writes to `pending_` and marks the signal dirty.
- `level()` reads the committed `level_`.
- `commit()` copies `pending_` to `level_`. Called by the Scheduler.

### Shutdown Order

1. Stop the 8284A (joins thread) -- no more `evaluate()` calls.
2. Delete fiber components (safe -- fibers are never resumed again).

## What Gets Modeled

Everything on the physical motherboard:
- **ICs**: 8088 CPU, 8284 clock gen, 8288 bus controller, 8259 PIC, 8253 PIT, 8237 DMA, 8255 PPI, 74S373 latches, 74S245 transceivers, 74S138 decoders, ROM chips
- **Passive components**: resistors, capacitors, DIP switches (SW1/SW2), jumpers
- **Power**: +5V, +12V, -5V, -12V rails from PSU -- modeled as enable signals, no power = nothing ticks
- **Connectors**: ISA slots (62-pin edge connectors), keyboard DIN, cassette, speaker
- **All signal lines**: address bus (A0-A19), data bus (D0-D7), control signals (~MEMR, ~MEMW, ~IOR, ~IOW, ALE, etc.), IRQ lines, DMA lines

## BRD-Driven Wiring

All motherboard wiring is sourced at runtime from the KiCad legacy BRD file (`assets/pcb/64_256KB_SYSTEM_BOARD_rev1_2a.brd`). No manual wiring code exists.

- **`BrdParser`** (`src/board/brd_parser.cpp`) parses the BRD file, extracting nets (`$EQUIPOT` sections) and component pin-to-net assignments (`$MODULE/$PAD` sections).
- **`Motherboard::wire_from_brd()`** iterates all 194 parsed components and wires each pin to its net's `Signal*`.
- **Net resolution**: Named nets (e.g. `CLK`, `A0`, `+5V`) are mapped to pre-declared `Signal` members via `build_net_map()` (~150 mappings). Anonymous nets (`N-000xxx`) get dynamically created `Signal` objects stored in `dynamic_signals_`.
- **Component registry**: `build_component_registry()` registers all board objects (sockets, ISA slots, DIP switches, passives, connectors) by ref designator for lookup during wiring.
- **Verification**: The `Multimeter::audit()` function re-parses the BRD and verifies that all pins on each net share the same `Signal*` pointer -- a full continuity check (305 nets, 1571 pins, 0 failures).

## Signal/Wire Abstraction

Implemented in `src/core/signal.h`:
- `Signal` class: named wire with tri-state logic (Low, High, Hi-Z), double-buffered (pending/committed). `connect(Component*)` adds the component to the signal's subscriber list.
- `Bus` class: bundle of N signal lines (e.g., address bus = 20 signals).

## VCC-Driven Power

Fibers don't execute until the board is powered on. The pattern is:
1. `power_on()` creates the fiber. First `resume()` enters `run()`.
2. The default `run()` calls `on_power_on()` then loops on `yield(); on_signal_change();`.
3. Active ICs (8088) override `run()` and `yield()` in a loop waiting for VCC High.
4. `Motherboard::power_on()` drives VCC High. The scheduler commits it and resumes fibers.
5. `Motherboard::power_off()` drives VCC Low. ICs detect VCC drop and exit their run loops.

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

18-pin DIP, generates the master clock. The only ThreadedComponent.

- **OSC** (pin 12): 14.31818 MHz oscillator output -- toggles every tick.
- **CLK** (pin 8): OSC / 3 = 4.77 MHz, 33% duty cycle (high 2 OSC half-periods, low 4).
- **PCLK** (pin 2): CLK / 2 = 2.38 MHz, 50% duty cycle.
- **RESET** (pin 10): Synchronized to CLK falling edge. Inverted RES input.
- **READY** (pin 5): Synchronized to CLK falling edge. RDY1 gated by ~AEN1.

Its thread converts to a fiber (`fiber_convert_thread()`) so it can switch to component fibers during `evaluate()`. Reverts back to a plain thread before returning.

## Future

- 3D visualization of the motherboard (separate concern, will come later)
- The architecture should be visualization-friendly: named signals, named components, physical positions can be layered on later
