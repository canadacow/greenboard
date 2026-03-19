# Bench -- IBM PC 5150 Motherboard Emulator Architecture

## Core Principle

Emulate the motherboard at the **interconnect level**. ICs are behaviorally emulated (black boxes), but every wire, trace, bus line, and signal between them is explicitly modeled. This includes passive components (resistors, capacitors, DIP switches, jumpers) and power rails.

## Execution Model: Fibers + One Thread

The simulation runs on a single OS thread. The 8284A clock generator owns that thread (via `std::jthread`) and acts as the crystal oscillator -- its spin loop IS the clock. All other ICs are evaluated on the 8284A's thread, cooperatively scheduled by the `Scheduler`. Most ICs are **CallbackComponents** (direct function calls). The 8088 CPU is the sole **FiberComponent** (resumed via context switch each cycle).

- The 8284A drives PCLK/READY/RESET each cycle, then calls `scheduler->evaluate(self)`.
- `evaluate()` resolves the current DAG permutation (based on bidirectional pin state), then calls `on_signal_change()` on each component in topological wave order.
- Both FiberComponents and CallbackComponents participate in the same DAG wave plan. CallbackComponents are invoked via direct function call; FiberComponents (8088) are resumed via fiber context switch. Both go through `on_signal_change()` in wave order.
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
  1. Evaluate bidir lambdas to determine current pin directions
  2. Compute DAG permutation key from bidir state
  3. Look up (or solve on cache miss) the topological wave plan
  4. For each wave: call on_signal_change() on every component in that wave
```

**Signal Enum** -1 is Low, 0 is HiZ and 1 is High. DO NOT FORGET THIS.

**One evaluate() = one full clock cycle.** Each component's `on_signal_change()` is called exactly once per cycle. There is no edge detection at the scheduler level -- components track their own `_prev_` state internally to detect rising/falling edges.

The scheduler builds a dependency DAG from pin declarations:
- `declare_input(pin)` -- permanent input edge (component depends on whoever drives this signal)
- `declare_output(pin)` -- permanent output edge (component drives this signal)
- `declare_async_input(pin)` -- reads the signal but creates no DAG edge (value from previous cycle)
- `declare_bidir_block(pins, dirs, lambda)` -- conditional edges that switch direction at runtime

Bidirectional pins (e.g. 74S245 data bus, DMA address pins) create multiple DAG permutations -- one per combination of directions. The bidir lambda runs at the start of each evaluate to determine the current direction (Input, Output, or HiZ). HiZ removes all DAG edges for those pins, effectively making the component invisible on that bus for that cycle. A bidir returning HiZ overrides any `declare_output` on the same pin.

Permutations are solved on demand (topological sort) and cached in an `unordered_map<uint64_t, WavePlan>`. On a DAG cycle, the scheduler dumps a Graphviz SVG (`wave_output/cycle_debug.svg`) showing the stuck components and conflicting edges in red.

### Bus Ownership and Transceiver Control

The 8288 bus controller and 8237A DMA controller are **authoritative** for all 74S245 bus transceivers. They call `set_driving()` directly on U8 (AD<->D), U12 (D<->MD), U13 (D<->XD), and U14 (cmd strobes) to set direction before the bidir lambda runs. This ensures the DAG permutation reflects the correct data flow direction for the current bus cycle.

- **CPU mode**: 8288 controls all four transceivers based on DT/~R and ~DEN. For memory reads, U12 is only enabled when A18=Low and A19=Low (RAM address range); ROM addresses leave U12 off so ROM data flows through U13 instead.
- **DMA mode**: 8237A takes over U14 (B->A direction so DMA's ~MEMR/~MEMW reach the system bus). U8/U13 go HiZ. U12 direction depends on transfer type (IO->mem write vs mem->IO read).

### Deferred Bus Operations (Pending Pattern)

Several peripherals (8253 PIT, 8237A DMA, ISA TestCard, 8K ROM) use a deferred read/write pattern. When a chip-select and strobe go active in the same cycle, the component sets a `_pending_` flag but does NOT read/write data yet -- the bus data hasn't propagated through the transceiver chain. On the next `on_signal_change()` call, the pending flag is consumed and the actual read/write executes with valid bus data.

This is necessary because the 8288's transceiver nudge and the address decode chain settle in the same evaluation cycle, but the actual data transfer through the 74S245 chain happens one wave later than the component's `on_signal_change()`.

### SignalPool

All signals are backed by a global `SignalPool` -- a single contiguous, 64-byte-aligned array of `Level` (enum class : uint8_t). No double buffering.

- `drive(lvl)`: Writes directly to `SignalPool::levels[idx]`.
- `level()`: Reads directly from `SignalPool::levels[idx]`.

Writes are immediately visible to all subsequent reads within the same evaluate cycle. The DAG wave ordering ensures producers run before consumers.

### Pin Validation (debug)

Compile-time `#define BENCH_PIN_VALIDATION` enables runtime checks on every `Pin::drive()` and `Pin::level()` call, verifying the active component has declared the correct direction for that pin. Catches missing or incorrect `declare_input`/`declare_output` calls. Disabled by default for performance.

### Thread Tuning (`src/host_platform/thread_util.h`)

Platform-agnostic API for OS thread priority and CPU affinity. Windows implementation in `thread_util_win32.cpp`.

```
thread_set_time_critical();   // THREAD_PRIORITY_TIME_CRITICAL on Windows
thread_pin_to_pcores();       // SetThreadAffinityMask to P-cores on Intel hybrid CPUs
```

P-core detection uses `GetSystemCpuSetInformation()` to find logical processors with the highest `EfficiencyClass` value. On non-hybrid systems, `thread_pin_to_pcores()` is a no-op.

Called at the top of `IC_8284A::run()` before fiber conversion.

### PSU Device (inside 8284A)

The 8284A contains a mock PSU driven from the main thread (`psu_power_on()`, `psu_power_off()`, `psu_nmi_raise()`, `psu_nmi_lower()`). On the clock thread, the PSU drives GND, VCC, RES, S0-S2, AEN, and NMI. This avoids any main-thread signal writes or scheduler calls. Cross-thread visibility of PSU commands relies on the main thread writing before `start()` or during a timed sleep; no atomics are used.

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
- `Signal` class: named wire with tri-state logic (Low, High, Hi-Z). `connect(Component*)` registers the component for pin declaration tracking (used by the DAG solver, not for push notification).
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

Contains a mock PSU device (driven from the main thread). On its thread, drives GND, VCC, RES, S0-S2, AEN, NMI. Sets thread priority to TIME_CRITICAL and pins to P-cores (Intel hybrid) before converting to a fiber (`fiber_convert_thread()`) for cooperative scheduling during `evaluate()`. Reverts back to a plain thread before returning.

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
| MSVC /GL /LTCG /Ob3 /GS- /Gw /Gy (whole-program optimization, aggressive inlining) | 45,577 | 9.7x |

Note: the last row is measured on a different benchmark configuration (with DMA and more ICs in the DAG) so is not directly comparable to the rows above.

Design principles:
- **No per-signal overhead**: `drive()` is a single store. No dirty flags, no subscriber notification, no atomic ops.
- **No double buffering**: DAG-based wave ordering guarantees producers run before consumers, eliminating the need for pending/current arrays and commit passes.
- **Minimize context switches**: Only the 8088 needs fibers. Everything else is a direct function call.
- **On-demand DAG solving**: Bidirectional pin permutations are solved on first encounter and cached, avoiding upfront enumeration of all 3^N combinations.
- **`class final` + LTCG**: All IC classes are marked `final`, enabling MSVC's whole-program optimizer to devirtualize `on_signal_change()` calls at link time.

## DMA Subsystem

The 8237A DMA controller (U35) and its supporting glue logic (U50, U52, U67, U98, U19, U62, U79, U49, U81, TD1) implement full 4-channel DMA with single/block/demand transfer modes. Channel 0 handles DRAM refresh via auto-init single transfers triggered by PIT channel 1. Channels 1-3 serve ISA peripherals.

DMA outputs (address, DACKs, HRQ, ~EOP, ~MEMR/~MEMW) feed back through the address decode chain to the 8237A's own inputs (~DMA_CS, CEN), creating DAG cycles. These are resolved with bidir blocks that return HiZ for signals stable during a given DMA phase. The 8237A uses deferred register writes (pending flag pattern) since bus data arrives one evaluation after ~IOW/~CS assert.

## Future

- 3D visualization of the motherboard (separate concern, will come later)
- The architecture should be visualization-friendly: named signals, named components, physical positions can be layered on later
