#pragma once
#include "core/types.h"
#include "core/signal.h"
#include "host_platform/fiber.h"
#include <functional>
#include <string>
#include <vector>

namespace bench {

// Base class for anything that sits in a socket on the motherboard.
//
// Two concrete subclasses model the two kinds of IC:
//
//   ThreadedComponent -- sequential/clocked ICs (8088, 8284A, 8288, 8259A, ...).
//     Has its own thread and mailbox. Woken asynchronously by signal changes.
//
//   InlineComponent -- combinational logic (74S373, 74S138, 74S245).
//     No thread. Evaluated synchronously by the Scheduler at each CLK edge.
//
class Component {
public:
    explicit Component(std::string name);
    virtual ~Component();

    const std::string& name() const { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    const std::string& description() const { return description_; }
    void set_description(std::string desc) { description_ = std::move(desc); }

    // Power control. Subclass semantics differ:
    //   ThreadedComponent: starts/stops the worker thread.
    //   InlineComponent: resets internal state (no thread).
    virtual void power_on() = 0;
    virtual void power_off() = 0;
    virtual bool is_powered() const = 0;

    // Pin direction declarations for wiring visualization and dependency graph.
    static constexpr int SLOT_WORDS = (SignalPool::MAX_SIGNALS + 63) / 64;
    void declare_input(Pin p)  { if (p.idx == 0) return; inputs_[p.idx / 64]  |= uint64_t(1) << (p.idx % 64); }
    void declare_output(Pin p) { if (p.idx == 0) return; outputs_[p.idx / 64] |= uint64_t(1) << (p.idx % 64); }

    // Async inputs are sampled by the IC at a future point (e.g. INTR, NMI,
    // READY, TEST on the 8088). They appear in the wiring graph as inputs
    // but create no ordering edges in the dependency DAG -- the value read
    // this cycle was driven in a previous cycle.
    void declare_async_input(Pin p) {
        if (p.idx == 0) return;
        inputs_[p.idx / 64]       |= uint64_t(1) << (p.idx % 64);
        async_inputs_[p.idx / 64] |= uint64_t(1) << (p.idx % 64);
    }

    const uint64_t* inputs()  const { return inputs_; }
    const uint64_t* outputs() const { return outputs_; }
    const uint64_t* async_inputs() const { return async_inputs_; }

    // Tri-state direction for bidir blocks (bit-flag values for composing masks).
    enum class BidirDir : uint8_t {
        HiZ    = 1,  // pins disconnected (no dependency either way)
        Input  = 2,  // component reads these pins
        Output = 4,  // component drives these pins
    };
    friend constexpr BidirDir operator|(BidirDir a, BidirDir b) {
        return BidirDir(uint8_t(a) | uint8_t(b));
    }
    friend constexpr bool operator&(BidirDir mask, BidirDir d) {
        return (uint8_t(mask) & uint8_t(d)) != 0;
    }

    // Bidirectional pin blocks whose direction changes at runtime.
    // The scheduler builds a DAG per direction permutation and selects at runtime.
    //
    // Three states: Output (drives out_mask, reads in_mask),
    //               Input  (reads out_mask, drives in_mask),
    //               HiZ    (pins removed from both eff_out and eff_in).
    //
    // `possible` is a bitmask of BidirDir values this block can be in.
    // The scheduler enumerates the Cartesian product of each block's
    // possible states, so the permutation count is the product of
    // per-block popcount(possible), not 3^N.
    //
    // For unpaired blocks (e.g. 8088 AD0-AD7), in_mask is all zeros.
    // For paired blocks (e.g. 74S245 A+B), out_mask and in_mask are anti-correlated.
    struct BidirBlock {
        uint64_t out_mask[SLOT_WORDS] = {};
        uint64_t in_mask[SLOT_WORDS]  = {};
        std::function<BidirDir()> direction;
        BidirDir possible = {};
    };

    // Single-sided bidir block. Default possible: Input | Output.
    void declare_bidir_block(std::initializer_list<Pin> pins,
                             BidirDir possible_states,
                             std::function<BidirDir()> dir_fn) {
        bidir_blocks_.emplace_back();
        auto& b = bidir_blocks_.back();
        for (auto p : pins)
            if (p.idx != 0)
                b.out_mask[p.idx / 64] |= uint64_t(1) << (p.idx % 64);
        b.direction = std::move(dir_fn);
        b.possible = possible_states;
    }
    void declare_bidir_block(std::initializer_list<Pin> pins, std::function<BidirDir()> dir_fn) {
        declare_bidir_block(pins, BidirDir::Input | BidirDir::Output, std::move(dir_fn));
    }

    // Paired bidir block with explicit possible states.
    // out_pins driven when direction()=Output, in_pins driven when Input.
    // HiZ: both sides disconnected.
    void declare_bidir_pair(std::initializer_list<Pin> out_pins,
                            std::initializer_list<Pin> in_pins,
                            BidirDir possible_states,
                            std::function<BidirDir()> dir_fn) {
        bidir_blocks_.emplace_back();
        auto& b = bidir_blocks_.back();
        for (auto p : out_pins)
            if (p.idx != 0)
                b.out_mask[p.idx / 64] |= uint64_t(1) << (p.idx % 64);
        for (auto p : in_pins)
            if (p.idx != 0)
                b.in_mask[p.idx / 64] |= uint64_t(1) << (p.idx % 64);
        b.direction = std::move(dir_fn);
        b.possible = possible_states;
    }

    const std::vector<BidirBlock>& bidir_blocks() const { return bidir_blocks_; }

protected:
    // Called when a connected signal changes.
    virtual void on_cycle(Fiber caller) {}
    virtual void on_power_on() {}
    virtual void on_power_off() {}

    // Each subclass routes itself into the correct Signal subscriber list.
    virtual void subscribe_to(Signal& sig) = 0;

    std::vector<Signal*> connected_signals_;

    friend class Signal;
    friend class Scheduler;
    friend class ISA_Bus;

private:
    std::string name_;
    std::string description_;
    uint64_t inputs_[SLOT_WORDS]       = {};
    uint64_t outputs_[SLOT_WORDS]      = {};
    uint64_t async_inputs_[SLOT_WORDS] = {};
    std::vector<BidirBlock> bidir_blocks_;
    friend class Scheduler;
};

} // namespace bench
