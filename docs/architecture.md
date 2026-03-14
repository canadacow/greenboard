# Bench -- IBM PC 5150 Motherboard Emulator Architecture

## Core Principle

Emulate the motherboard at the **interconnect level**. ICs are behaviorally emulated (black boxes), but every wire, trace, bus line, and signal between them is explicitly modeled. This includes passive components (resistors, capacitors, DIP switches, jumpers) and power rails.

## Execution Model: Fibers + One Thread

The simulation runs on a single OS thread. The 8284A clock generator owns that thread (via `std::jthread`) and acts as the crystal oscillator -- its spin loop IS the clock. All other ICs run as **Windows Fibers** on the 8284A's thread, cooperatively scheduled by the `Scheduler`.

- The 8284A drives PCLK/READY/RESET each cycle, then calls `scheduler->evaluate(self)`.
- `evaluate()` resolves the current DAG permutation (based on bidirectional pin state), then calls `on_signal_change()` on each component in topological wave order.
- FiberComponents are resumed via context switch; CallbackComponents are invoked directly.
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
  +-- CallbackComponent    -- all non-yielding ICs (8288, 74S373, 74S138, 74S245, 8259A, 8253, etc.)
  +-- FiberComponent       -- ICs that suspend mid-operation (8088)
```

Two evaluation modes:

1. **CallbackComponent** (`src/core/callback_component.h`): ICs that complete all work in a single `on_signal_change()` call. Invoked via direct function call -- no fiber overhead. Used for all non-CPU ICs: bus controller (8288), glue logic (74S373, 74S138, 74S245, 74S20, 74S175), peripherals (8259A, 8253, 8255A, 8237A), ROM, DRAM.

2. **FiberComponent** (`src/core/fiber_component.h`): ICs that need to suspend mid-operation via `yield()`. Context switch on every `evaluate()`. Reserved for the 8088 CPU, which yields at each T-state boundary.
   - `power_on()`: Creates a fiber. First `resume()` enters `run()`.
   - `power_off()`: Calls `on_power_off()`, deletes the fiber.
   - `resume(caller)`: Switches to the fiber. The fiber runs until it calls `yield()`.
   - Default `run()`: Calls `on_power_on()`, then loops `yield(); on_signal_change();`.
   - Active ICs (8088) override `run()` with their own execution loop.

### Scheduler (`src/core/scheduler.h`)

Lightweight, non-owning. Called by the 8284A at each CLK cycle.

```
evaluate(Fiber caller):
  1. Compute DAG permutation from bidirectional pin state
  2. Look up (or solve on cache miss) the topological wave plan
  3. For each wave: call on_signal_change() on every component in that wave
```

The scheduler builds a dependency DAG from pin declarations (`declare_input`/`declare_output`). Bidirectional pins (e.g. 74S245 data bus) create multiple DAG permutations -- one per combination of directions. Permutations are solved on demand and cached in an `unordered_map<int, WavePlan>`.

### SignalPool

All signals are backed by a global `SignalPool` -- a single contiguous, 64-byte-aligned array of `Level` (enum class : uint8_t). No double buffering.

- `drive(lvl)`: Writes directly to `SignalPool::levels[idx]`.
- `level()`: Reads directly from `SignalPool::levels[idx]`.

Writes are immediately visible to all subsequent reads within the same evaluate cycle. The DAG wave ordering ensures producers run before consumers.

### Pin Validation (debug)

Compile-time `#define BENCH_PIN_VALIDATION` enables runtime checks on every `Pin::drive()` and `Pin::level()` call, verifying the active component has declared the correct direction for that pin. Catches missing or incorrect `declare_input`/`declare_output` calls. Disabled by default for performance.

### PSU Device (inside 8284A)

The 8284A contains a mock PSU driven via atomics from the main thread (`psu_power_on()`, `psu_power_off()`, `psu_nmi_raise()`, `psu_nmi_lower()`). On the clock thread, the PSU drives GND, VCC, RES, S0-S2, AEN, and NMI. This avoids any main-thread signal writes or scheduler calls.

### Shutdown Order

1. `psu_power_off()` -- 8284A drops VCC/RES on its thread, stops oscillating.
2. `power_off()` on 8284A (joins thread) -- no more `evaluate()` calls.
3. Delete fiber components (safe -- fibers are never resumed again).

## What Gets Modeled

Everything on the physical motherboard:
- **ICs**: 8088 CPU, 8284 clock gen, 8288 bus controller, 8259 PIC, 8253 PIT, 8237 DMA, 8255 PPI, 74S373 latches, 74S245 transceivers, 74S138 decoders, 74S175 flip-flops, 74S20 NAND gates, ROM chips, DRAM
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
- `Signal` class: named wire with tri-state logic (Low, High, Hi-Z). `connect(Component*)` adds the component to the signal's subscriber list.
- `Bus` class: bundle of N signal lines (e.g., address bus = 20 signals).
- `Pin` struct: lightweight index-based handle into the SignalPool. `drive()`/`level()` use static array base + index.
- `PinBlock<N>` struct: contiguous block of N pool slots for bulk read/write via memcpy.

## VCC-Driven Power

Components don't execute until the board is powered on. The pattern is:
1. `power_on()` initializes the component (creates fiber for FiberComponent, sets powered flag for others).
2. The 8284A's PSU drives VCC High on the clock thread. The scheduler evaluates all components.
3. Active ICs (8088) override `run()` and `yield()` in a loop waiting for VCC High.
4. `psu_power_off()` drops VCC. ICs detect VCC drop and exit their run loops.

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

- **PCLK** (pin 2): CLK / 2 = 2.38 MHz, 50% duty cycle.
- **RESET** (pin 10): Inverted RES input.
- **READY** (pin 5): RDY1 gated by ~AEN1.

Contains a mock PSU device (driven via atomics from the main thread). On its thread, drives GND, VCC, RES, S0-S2, AEN, NMI. Converts to a fiber (`fiber_convert_thread()`) so it can switch to component fibers during `evaluate()`. Reverts back to a plain thread before returning.

## Performance

Key optimizations and their measured impact (64-bit increment benchmark, 5-second NMI-timed run):

| Change | inc/s | Speedup |
|--------|------:|--------:|
| Baseline (atomics, dirty tracking, all fibers) | 4,722 | 1.0x |
| Remove atomics from Signal (single-threaded hot path) | 7,695 | 1.6x |
| SignalPool (contiguous arrays, no dirty tracking) + AVX2 commit | 9,801 | 2.1x |
| CallbackComponent (eliminate fiber context switches for 8288/PIC) | 11,643 | 2.5x |
| Convert all non-yielding ICs to callback | 18,377 | 3.9x |
| Remove double buffering (single array, no commit) + DAG wave ordering | 50,499 | 10.7x |

Design principles:
- **No per-signal overhead**: `drive()` is a single store. No dirty flags, no subscriber notification, no atomic ops.
- **No double buffering**: DAG-based wave ordering guarantees producers run before consumers, eliminating the need for pending/current arrays and commit passes.
- **Minimize context switches**: Only the 8088 needs fibers. Everything else is a direct function call.
- **On-demand DAG solving**: Bidirectional pin permutations are solved on first encounter and cached, avoiding upfront enumeration of all 3^N combinations.

## Future

- 3D visualization of the motherboard (separate concern, will come later)
- The architecture should be visualization-friendly: named signals, named components, physical positions can be layered on later
