#pragma once
// CFGTracer -- execution trace recorder for offline reverse engineering.
//
// Records the minimum set of facts needed to reconstruct a session offline:
//
//   1. CFG      -- each instruction address, logged on FIRST visit only, plus
//                  every non-sequential control transfer (branch edge).
//   2. Writes   -- every byte that becomes memory, from any source (CPU or
//                  DMA), stamped with the instruction that was executing.
//   3. Port in  -- every I/O read result. These are inputs to the machine and
//                  are NOT derivable from anything else in the trace.
//
// Memory READS are deliberately not recorded: given the full write log, the
// value at any address at any point in the trace is exactly the last write to
// that address at or before that point. Reads carry no information that the
// writes do not already carry.
//
// There is no initial memory image because tracing starts at RESET. Every
// meaningful byte in the machine got there via a recorded write.
//
// Overlays / self-modifying code: a byte that is written after having been
// executed invalidates the CFG node at that address. The dirty bitmap tracks
// this; the next execution there produces a NEW node rather than aliasing onto
// the old one. Two overlays sharing an address stay distinct in the graph.
//
// Threading: the entire simulation runs on the 8284A's thread (see
// docs/architecture.md). The CPU coroutine and the DRAM/ISA on_cycle() calls
// are the same thread, ordered by the DAG wave plan. No synchronization is
// needed or used here.

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bench {

class CFGTracer {
public:
    // Physical address space is 20 bits.
    static constexpr uint32_t ADDR_SPACE = 1u << 20;

    struct WriteRec {
        uint64_t instr;    // instr_count_ at the time of the write
        uint32_t addr;     // 20-bit physical address
        uint16_t cs;       // CPU position when the byte landed
        uint16_t ip;
        uint8_t  data;
    };

    // Full register state at every traced instruction.
    //
    // Recorded for every instruction outside BIOS and interrupt handlers, in
    // execution order, so any point in the run can be resumed exactly: load
    // these registers, point a simulator at cs:ip, and it continues as it did
    // here. That makes a routine re-runnable standalone instead of having to
    // reconstruct its inputs by inference.
    //
    // The segment registers are the load-bearing part -- reading a data table
    // out of the trace means knowing what DS actually held, and guessing it
    // from nearby writes gets the address wrong.
    //
    // 28 bytes per instruction. There is no index field: the record's
    // position corresponds to a position in the state timeline, and `instr`
    // ties it back to the execution timeline.
    struct StateRec {
        uint64_t instr;
        uint32_t addr;      // physical address of the instruction
        uint16_t ax, cx, dx, bx;
        uint16_t sp, bp, si, di;
        uint16_t es, cs, ss, ds;
        uint16_t flags;
        uint16_t _pad;
    };

    // One entry per executed instruction: where the CPU was, in order.
    //
    // The CFG says which instructions exist and how they connect; it cannot
    // say where execution was at a given moment, because a node records only
    // its FIRST execution. This does -- it is what makes "scrub to instant N
    // and see the executing instruction" and "step one instruction" possible.
    //
    // Deliberately just the address: the instruction count is the array index
    // (entry k is instruction k), and everything else about the instruction is
    // already in the node table.
    struct ExecRec {
        uint32_t addr;   // 20-bit physical address of the opcode byte
    };

    struct PortRec {
        uint64_t instr;
        uint16_t port;
        uint16_t cs;
        uint16_t ip;
        uint8_t  data;
        uint8_t  is_write;  // 0 = IN (machine input), 1 = OUT
    };

    // A CFG node: one instruction, recorded the first time it executes (or the
    // first time it executes after being overwritten).
    struct Node {
        uint32_t cs_ip;      // 20-bit physical address of the opcode byte
        uint16_t cs;
        uint16_t ip;
        uint32_t gen;        // how many times this address had been dirtied
        uint64_t first_seen; // instr_count_ on first execution
        uint64_t hits;       // execution count
    };

    CFGTracer();

    // ---- Configuration (call before power-on) ----

    // Exclude a physical address range from the CFG. Instructions executing
    // inside an excluded range produce no nodes; a call into and back out of
    // the range collapses to a single edge. Writes and port reads are still
    // recorded regardless -- BIOS writes (disk loads, BDA) are exactly the
    // events we care most about.
    void exclude_range(uint32_t lo, uint32_t hi_exclusive);

    // Default exclusion: motherboard ROM at F0000-FFFFF.
    void exclude_bios_default() { exclude_range(0xF0000, 0x100000); }

    // ---- Hot-path hooks ----

    // Called at the top of each instruction, before any fetch.
    //
    // Records the node (first visit only) and the edge from the previous
    // instruction. `in_handler` is true while the CPU is inside an interrupt
    // handler: the execution timeline still records those instructions, so
    // its index stays equal to the CPU's instruction count, but no CFG node
    // or edge is built for them.
    //
    // Without that gate, a handler entered between two instructions looks
    // like a control-flow successor of whatever it interrupted -- inventing
    // edges that never existed, giving conditional jumps impossible fan-out,
    // and fragmenting straight-line code into single-instruction blocks.
    void on_instruction(uint32_t cs_ip, uint16_t cs, uint16_t ip,
                        uint64_t instr, bool in_handler = false);

    // Called at every traced instruction, before it executes. `regs16` points
    // at the CPU's 16-bit register array; `flags` is the packed FLAGS word.
    // Not called for BIOS or interrupt-handler instructions.
    void on_state(uint32_t addr, const uint16_t* regs16, uint16_t flags,
                  uint64_t instr);

    // Whether an address is in an excluded region, so the CPU can skip
    // building the flags word for instructions that will not be recorded.
    bool is_excluded(uint32_t addr) const { return excluded(addr); }

    // Called from DRAM / ISA RAM / video when a byte lands in memory.
    // Fires for every write regardless of who drove the bus -- a DMA
    // transfer and a CPU store are the same event as far as the log is
    // concerned.
    void on_write(uint32_t addr, uint8_t data,
                  uint16_t cs, uint16_t ip, uint64_t instr);

    // Called when an IN instruction completes. These are machine INPUTS --
    // nothing else in the trace predicts them.
    void on_port_read(uint16_t port, uint8_t data,
                      uint16_t cs, uint16_t ip, uint64_t instr);

    // Called when an OUT instruction completes. Derivable by replaying the
    // CFG, but recorded so the trace can be read without a CPU model (e.g.
    // the video mode register that says how to interpret framebuffer bytes).
    void on_port_write(uint16_t port, uint8_t data,
                       uint16_t cs, uint16_t ip, uint64_t instr);

    // ---- Output ----

    // Write the trace to disk. Called at power-off.
    bool dump(const std::string& path);

    // ---- Stats (for UI / logging) ----
    size_t node_count()  const { return nodes_.size(); }
    size_t edge_count()  const { return edges_.size(); }
    size_t write_count() const { return writes_.size(); }
    size_t port_count()  const { return ports_.size(); }
    size_t exec_count()  const { return exec_.size(); }
    size_t state_count() const { return states_.size(); }
    uint64_t dirty_events() const { return dirty_events_; }
    uint64_t handler_instrs() const { return handler_instrs_; }

private:
    bool excluded(uint32_t addr) const {
        for (const auto& r : excludes_)
            if (addr >= r.lo && addr < r.hi) return true;
        return false;
    }

    struct Range { uint32_t lo, hi; };
    std::vector<Range> excludes_;

    // CFG. Key packs the address with its generation so that an overlay
    // loaded at an address a previous overlay occupied gets its own node.
    static uint64_t node_key(uint32_t cs_ip, uint32_t gen) {
        return (uint64_t(gen) << 32) | cs_ip;
    }
    std::unordered_map<uint64_t, Node> nodes_;

    // Observed control transfers, packed (from << 32) | to.
    std::unordered_set<uint64_t> edges_;

    // Per-byte generation counter, bumped when an executed byte is written.
    // Sized to the full address space; 4 MB, allocated once.
    std::vector<uint32_t> gen_;

    // Per-byte flag: has this address ever been executed? Only executed bytes
    // need their generation bumped on write, so data writes cost one test.
    std::vector<uint8_t> executed_;

    std::vector<WriteRec> writes_;
    std::vector<PortRec>  ports_;

    // Execution timeline, one entry per instruction in execution order.
    // Index == instruction count, so no timestamp is stored.
    std::vector<uint32_t> exec_;

    // Register state per traced instruction, in execution order.
    std::vector<StateRec> states_;

    // Previous non-excluded instruction, for edge recording across an
    // excluded region (e.g. game code -> BIOS -> back to game code).
    uint32_t prev_visible_ = ~0u;
    bool     in_excluded_  = false;

    uint64_t dirty_events_ = 0;

    // Instructions executed inside interrupt handlers. On the timeline but
    // deliberately absent from the CFG.
    uint64_t handler_instrs_ = 0;
};

} // namespace bench
