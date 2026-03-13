#pragma once
#include "core/signal.h"
#include "core/bus_controller_component.h"
#include "core/callback_component.h"
#include "core/fiber_component.h"
#include "core/inline_component.h"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

namespace bench {

// Central synchronous evaluator -- the beating heart of the simulation.
//
// Called by the 8284A at each CLK edge. Commits all signals via
// SignalPool::commit(), evaluates inline ICs to fixed-point,
// then resumes all fiber components cooperatively.
//
// Callback execution order is auto-resolved from pin declarations:
// resolve() builds a dependency DAG (A.outputs & B.inputs != 0 => A before B),
// topologically sorts into waves, and inserts a commit() between each wave.
class Scheduler {
public:
    void register_inline(InlineComponent* ic) {
        assert(inline_count_ < MAX_INLINES && "Scheduler: too many inline ICs");
        inlines_[inline_count_++] = ic;
    }

    void register_bus_controller(BusControllerComponent* bc) {
        assert(bus_ctrl_count_ < MAX_BUS_CTRLS && "Scheduler: too many bus controllers");
        bus_ctrls_[bus_ctrl_count_++] = bc;
    }

    void register_callback(CallbackComponent* cc) {
        assert(callback_count_ < MAX_CALLBACKS && "Scheduler: too many callback ICs");
        callbacks_[callback_count_++] = cc;
    }

    void register_fiber(FiberComponent* fc) {
        assert(fiber_count_ < MAX_FIBERS && "Scheduler: too many fiber components");
        fibers_[fiber_count_++] = fc;
    }

    // Register a component for visualization only (e.g. threaded 8284A).
    void register_visual(Component* c) {
        assert(visual_count_ < MAX_VISUALS && "Scheduler: too many visual components");
        visuals_[visual_count_++] = c;
    }

    // Build the callback dependency graph and topologically sort into waves.
    // Call once after all callbacks are registered, before the first evaluate().
    void resolve() {
        const int n = callback_count_;
        if (n == 0) { resolved_ = true; return; }

        constexpr int W = Component::SLOT_WORDS;

        // Build adjacency: depends[b][a] = true means b depends on a
        // (a's outputs overlap b's inputs).
        bool depends[MAX_CALLBACKS][MAX_CALLBACKS] = {};
        for (int a = 0; a < n; ++a) {
            for (int b = 0; b < n; ++b) {
                if (a == b) continue;
                const uint64_t* a_out = callbacks_[a]->outputs();
                const uint64_t* b_in  = callbacks_[b]->inputs();
                for (int w = 0; w < W; ++w) {
                    if (a_out[w] & b_in[w]) {
                        depends[b][a] = true;
                        break;
                    }
                }
            }
        }

        // Kahn's algorithm -- topological sort with level assignment.
        int in_deg[MAX_CALLBACKS] = {};
        int level[MAX_CALLBACKS] = {};
        for (int b = 0; b < n; ++b)
            for (int a = 0; a < n; ++a)
                if (depends[b][a]) ++in_deg[b];

        int queue[MAX_CALLBACKS];
        int front = 0, back = 0;
        for (int i = 0; i < n; ++i)
            if (in_deg[i] == 0) queue[back++] = i;

        int sorted = 0;
        while (front < back) {
            int u = queue[front++];
            ++sorted;
            for (int v = 0; v < n; ++v) {
                if (!depends[v][u]) continue;
                level[v] = std::max(level[v], level[u] + 1);
                if (--in_deg[v] == 0) queue[back++] = v;
            }
        }
        assert(sorted == n && "Scheduler::resolve: cycle in callback dependencies");

        // Populate waves.
        num_waves_ = 0;
        for (int i = 0; i < n; ++i)
            if (level[i] + 1 > num_waves_) num_waves_ = level[i] + 1;

        for (int w = 0; w < num_waves_; ++w)
            wave_counts_[w] = 0;

        for (int i = 0; i < n; ++i) {
            int w = level[i];
            waves_[w][wave_counts_[w]++] = callbacks_[i];
        }

        for (int w = 0; w < num_waves_; ++w) {
            spdlog::info("[Scheduler] wave {}: {} callbacks", w, wave_counts_[w]);
            for (int i = 0; i < wave_counts_[w]; ++i)
                spdlog::info("[Scheduler]   - {}", waves_[w][i]->name());
        }

        dump_dot("callback_graph");
        resolved_ = true;
    }

    // Write a Graphviz DOT file showing ALL registered components with
    // per-pin ports and directed edges for each signal connecting an output
    // to an input. Flat layout -- no clustering by scheduling category.
    void dump_dot(const char* base) {
        std::string dot_path = std::string(base) + ".dot";
        std::string svg_path = std::string(base) + ".svg";
        FILE* f = std::fopen(dot_path.c_str(), "w");
        if (!f) return;

        // Gather every registered component into a flat list.
        std::vector<Component*> all;
        for (int i = 0; i < visual_count_;    ++i) all.push_back(visuals_[i]);
        for (int i = 0; i < fiber_count_;     ++i) all.push_back(fibers_[i]);
        for (int i = 0; i < bus_ctrl_count_;  ++i) all.push_back(bus_ctrls_[i]);
        for (int i = 0; i < inline_count_;    ++i) all.push_back(inlines_[i]);
        for (int i = 0; i < callback_count_;  ++i) all.push_back(callbacks_[i]);
        int total = static_cast<int>(all.size());

        std::fprintf(f, "digraph component_graph {\n");
        std::fprintf(f, "  rankdir=LR;\n");
        std::fprintf(f, "  node [shape=none fontname=\"Consolas\" fontsize=10];\n");
        std::fprintf(f, "  edge [fontname=\"Consolas\" fontsize=8 color=\"#555555\"];\n");
        std::fprintf(f, "  graph [nodesep=0.4 ranksep=1.2];\n\n");

        // Collect per-component pin lists: in-only, out-only, bidirectional (io).
        struct PinInfo { int slot; const char* name; };
        std::vector<std::vector<PinInfo>> in_pins(total), out_pins(total), io_pins(total);

        for (int i = 0; i < total; ++i) {
            auto* c = all[i];
            for (int s = 1; s < SignalPool::count; ++s) {
                const char* nm = SignalPool::names[s];
                if (!nm) continue;
                bool is_in  = (c->inputs()[s / 64]  >> (s % 64)) & 1;
                bool is_out = (c->outputs()[s / 64] >> (s % 64)) & 1;
                if (is_in && is_out) io_pins[i].push_back({s, nm});
                else if (is_in)      in_pins[i].push_back({s, nm});
                else if (is_out)     out_pins[i].push_back({s, nm});
            }
        }

        // Uniform header color for all components.
        const char* hdr_color = "#336699";

        // Emit each component as an HTML table node.
        for (int idx = 0; idx < total; ++idx) {
            auto* c = all[idx];
            int ni  = static_cast<int>(in_pins[idx].size());
            int nio = static_cast<int>(io_pins[idx].size());
            int no  = static_cast<int>(out_pins[idx].size());
            int cols = (nio > 0) ? 5 : 3;
            int rows = std::max({ni, nio, no, 1});
            std::fprintf(f, "  n%d [label=<\n", idx);
            std::fprintf(f, "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\">\n");
            if (!c->description().empty())
                std::fprintf(f, "      <TR><TD COLSPAN=\"%d\" BGCOLOR=\"%s\">"
                                "<FONT COLOR=\"white\"><I>%s</I></FONT></TD></TR>\n",
                             cols, hdr_color, c->description().c_str());
            std::fprintf(f, "      <TR><TD COLSPAN=\"%d\" BGCOLOR=\"%s\">"
                            "<FONT COLOR=\"white\"><B>%s</B></FONT></TD></TR>\n",
                         cols, hdr_color, c->name().c_str());
            // Column headers.
            std::fprintf(f, "      <TR>"
                            "<TD BGCOLOR=\"#dddddd\"><B>in</B></TD>"
                            "<TD></TD>");
            if (nio > 0)
                std::fprintf(f, "<TD BGCOLOR=\"#dddddd\"><B>io</B></TD>"
                                "<TD></TD>");
            std::fprintf(f, "<TD BGCOLOR=\"#dddddd\"><B>out</B></TD></TR>\n");
            for (int r = 0; r < rows; ++r) {
                std::fprintf(f, "      <TR>");
                if (r < ni)
                    std::fprintf(f, "<TD PORT=\"i%d\" BGCOLOR=\"#eeffee\" ALIGN=\"LEFT\">%s</TD>",
                                 in_pins[idx][r].slot, in_pins[idx][r].name);
                else
                    std::fprintf(f, "<TD></TD>");
                std::fprintf(f, "<TD>  </TD>");
                if (nio > 0) {
                    if (r < nio)
                        std::fprintf(f, "<TD PORT=\"b%d\" BGCOLOR=\"#fff3dd\" ALIGN=\"CENTER\">%s</TD>",
                                     io_pins[idx][r].slot, io_pins[idx][r].name);
                    else
                        std::fprintf(f, "<TD></TD>");
                    std::fprintf(f, "<TD>  </TD>");
                }
                if (r < no)
                    std::fprintf(f, "<TD PORT=\"o%d\" BGCOLOR=\"#ffeeee\" ALIGN=\"RIGHT\">%s</TD>",
                                 out_pins[idx][r].slot, out_pins[idx][r].name);
                else
                    std::fprintf(f, "<TD></TD>");
                std::fprintf(f, "</TR>\n");
            }
            if (ni == 0 && nio == 0 && no == 0)
                std::fprintf(f, "      <TR><TD COLSPAN=\"%d\"><I>(no pins declared)</I></TD></TR>\n", cols);
            std::fprintf(f, "    </TABLE>>];\n\n");
        }

        // Helper: find a slot in a pin list.
        auto has_slot = [](const std::vector<PinInfo>& v, int slot) {
            for (auto& p : v) if (p.slot == slot) return true;
            return false;
        };

        // Edges. For each pair (a,b) and shared signal slot:
        //   out(a) -> in(b)   : directed arrow
        //   out(a) -> io(b)   : directed arrow
        //   io(a)  -> in(b)   : directed arrow
        //   io(a)  <-> io(b)  : double arrow (emit once, a<b)
        for (int a = 0; a < total; ++a) {
            for (int b = 0; b < total; ++b) {
                if (a == b) continue;
                for (auto& op : out_pins[a])
                    if (has_slot(in_pins[b], op.slot))
                        std::fprintf(f, "  n%d:o%d -> n%d:i%d;\n", a, op.slot, b, op.slot);
                for (auto& op : out_pins[a])
                    if (has_slot(io_pins[b], op.slot))
                        std::fprintf(f, "  n%d:o%d -> n%d:b%d;\n", a, op.slot, b, op.slot);
                for (auto& bp : io_pins[a])
                    if (has_slot(in_pins[b], bp.slot))
                        std::fprintf(f, "  n%d:b%d -> n%d:i%d;\n", a, bp.slot, b, bp.slot);
                if (a < b)
                    for (auto& bp : io_pins[a])
                        if (has_slot(io_pins[b], bp.slot))
                            std::fprintf(f, "  n%d:b%d -> n%d:b%d [dir=both color=\"#cc8800\"];\n",
                                         a, bp.slot, b, bp.slot);
            }
        }

        std::fprintf(f, "}\n");
        std::fclose(f);
        spdlog::info("[Scheduler] wrote {}", dot_path);

        // Try to render SVG.
        std::string cmd = "\"C:/Program Files/Graphviz/bin/dot.exe\" -Tsvg " + dot_path + " -o " + svg_path + " 2>&1";
        if (std::system(cmd.c_str()) == 0)
            spdlog::info("[Scheduler] rendered {}", svg_path);
        else
            spdlog::warn("[Scheduler] dot not found -- SVG not rendered");
    }

    // Half-cycle flag: set by the 8088 before yielding.
    // The 8284A checks this after evaluate() returns.
    bool half_cycle_requested() const { return half_cycle_; }
    void request_half_cycle() { half_cycle_ = true; }
    void clear_half_cycle() { half_cycle_ = false; }

    // Commit + bus controllers + inlines (fixed-point) + callback waves + fibers.
    void evaluate(Fiber caller, bool rising = true, bool falling = true) {
        evaluate_no_wake(rising, falling);
        for (int i = 0; i < fiber_count_; ++i)
            fibers_[i]->resume(caller);
    }

    // Commit + bus controllers + settle inlines + callback waves. No fiber resume.
    //
    // Order matches real hardware propagation within a clock period:
    //   1. Commit pending signals (8088 status lines become visible)
    //   2. Bus controllers (8288 decodes S0-S2, drives ~IOW/~MEMR/ALE/etc.)
    //      -- 8288 uses drive_immediate() so outputs are in current[] already
    //   3. Commit + settle inlines (74S373 latches address, 74S138 decodes ~CS)
    //   4. Callback waves -- each wave followed by commit(), so producers'
    //      outputs are visible to consumer waves.
    void evaluate_no_wake(bool rising = true, bool falling = true) {
        SignalPool::commit();
        for (int i = 0; i < bus_ctrl_count_; ++i)
            bus_ctrls_[i]->on_signal_change(rising, falling);

        // Phase 2: Fixed-point inline IC evaluation.
        for (;;)
        {
            for (int i = 0; i < inline_count_; ++i)
                inlines_[i]->on_signal_change(rising, falling);

            if (!SignalPool::commit()) break;
        }

        // Phase 3: Callback waves (resolved dependency order).
        if (resolved_) {
            for (int w = 0; w < num_waves_; ++w) {
                for (int i = 0; i < wave_counts_[w]; ++i)
                    waves_[w][i]->on_signal_change(rising, falling);
                SignalPool::commit();
            }
        } else {
            // Fallback: no resolve() called, run flat (legacy behavior).
            for (int i = 0; i < callback_count_; ++i)
                callbacks_[i]->on_signal_change(rising, falling);
        }
    }

private:
    bool half_cycle_ = false;
    bool resolved_ = false;

    static constexpr int MAX_BUS_CTRLS = 4;
    static constexpr int MAX_INLINES = 32;
    static constexpr int MAX_CALLBACKS = 32;
    static constexpr int MAX_FIBERS = 256;
    static constexpr int MAX_WAVES = 8;

    BusControllerComponent* bus_ctrls_[MAX_BUS_CTRLS] = {};
    int bus_ctrl_count_ = 0;

    InlineComponent* inlines_[MAX_INLINES] = {};
    int inline_count_ = 0;

    CallbackComponent* callbacks_[MAX_CALLBACKS] = {};
    int callback_count_ = 0;

    // Resolved callback waves (populated by resolve()).
    CallbackComponent* waves_[MAX_WAVES][MAX_CALLBACKS] = {};
    int wave_counts_[MAX_WAVES] = {};
    int num_waves_ = 0;

    FiberComponent* fibers_[MAX_FIBERS] = {};
    int fiber_count_ = 0;

    static constexpr int MAX_VISUALS = 8;
    Component* visuals_[MAX_VISUALS] = {};
    int visual_count_ = 0;
};

} // namespace bench
