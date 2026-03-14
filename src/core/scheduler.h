#pragma once
#include "core/signal.h"
#include "core/bus_controller_component.h"
#include "core/callback_component.h"
#include "core/fiber_component.h"
#include "core/inline_component.h"
#include <array>
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

        // Build adjacency: depends[b][a] = true means b depends on a.
        // Uses effective outputs (output & ~input) to exclude bidirectional pins,
        // and effective inputs (input & ~async) to exclude cross-cycle signals.
        bool depends[MAX_CALLBACKS][MAX_CALLBACKS] = {};
        for (int a = 0; a < n; ++a) {
            for (int b = 0; b < n; ++b) {
                if (a == b) continue;
                for (int w = 0; w < W; ++w) {
                    uint64_t eff_out = callbacks_[a]->outputs()[w] & ~callbacks_[a]->inputs()[w];
                    uint64_t eff_in  = callbacks_[b]->inputs()[w]  & ~callbacks_[b]->async_inputs()[w];
                    if (eff_out & eff_in) {
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
        if (sorted != n) {
            spdlog::critical("[Scheduler] cycle in callback dependencies (sorted {} of {})", sorted, n);
            for (int i = 0; i < n; ++i)
                if (in_deg[i] > 0)
                    spdlog::critical("[Scheduler]   stuck: {} (in_deg={})", callbacks_[i]->name(), in_deg[i]);
            std::_Exit(1);
        }

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
        dump_unified_waves();
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

    // Prototype: topologically sort ALL evaluable components (bus controllers +
    // inlines + callbacks) using effective outputs (output & ~input) to break
    // bidirectional cycles. Logs wave assignment and dumps a DOT/SVG graph.
    // Does NOT change evaluation order -- purely for visualization/planning.
    void dump_unified_waves() {
        // Collect ALL components into a flat list (fibers included).
        std::vector<Component*> evals;
        for (int i = 0; i < fiber_count_;     ++i) evals.push_back(fibers_[i]);
        for (int i = 0; i < bus_ctrl_count_;  ++i) evals.push_back(bus_ctrls_[i]);
        for (int i = 0; i < inline_count_;    ++i) evals.push_back(inlines_[i]);
        for (int i = 0; i < callback_count_;  ++i) evals.push_back(callbacks_[i]);
        const int n = static_cast<int>(evals.size());
        if (n == 0) return;

        constexpr int W = Component::SLOT_WORDS;

        // Effective outputs: output-only pins (exclude bidirectional).
        // Effective inputs: exclude async inputs (cross-cycle, no ordering).
        std::vector<std::array<uint64_t, W>> eff_out(n), eff_in(n);
        for (int i = 0; i < n; ++i)
            for (int w = 0; w < W; ++w) {
                eff_out[i][w] = evals[i]->outputs()[w] & ~evals[i]->inputs()[w];
                eff_in[i][w]  = evals[i]->inputs()[w]  & ~evals[i]->async_inputs()[w];
            }

        // Build adjacency: depends[b][a] means b depends on a.
        std::vector<std::vector<bool>> depends(n, std::vector<bool>(n, false));
        for (int a = 0; a < n; ++a)
            for (int b = 0; b < n; ++b) {
                if (a == b) continue;
                for (int w = 0; w < W; ++w)
                    if (eff_out[a][w] & eff_in[b][w]) {
                        depends[b][a] = true;
                        // Log the signals that cause this edge.
                        uint64_t overlap = eff_out[a][w] & eff_in[b][w];
                        for (int bit = 0; bit < 64; ++bit) {
                            if ((overlap >> bit) & 1) {
                                int slot = w * 64 + bit;
                                const char* nm = (slot < SignalPool::count) ? SignalPool::names[slot] : "?";
                                spdlog::info("[unified]   {} -> {} via {}",
                                             evals[a]->name(), evals[b]->name(), nm ? nm : "?");
                            }
                        }
                        break;
                    }
            }

        // Kahn's algorithm with level assignment.
        std::vector<int> in_deg(n, 0), level(n, 0);
        for (int b = 0; b < n; ++b)
            for (int a = 0; a < n; ++a)
                if (depends[b][a]) ++in_deg[b];

        std::vector<int> queue;
        for (int i = 0; i < n; ++i)
            if (in_deg[i] == 0) queue.push_back(i);

        int front = 0, sorted = 0;
        while (front < static_cast<int>(queue.size())) {
            int u = queue[front++];
            ++sorted;
            for (int v = 0; v < n; ++v) {
                if (!depends[v][u]) continue;
                level[v] = std::max(level[v], level[u] + 1);
                if (--in_deg[v] == 0) queue.push_back(v);
            }
        }

        if (sorted != n) {
            spdlog::critical("[Scheduler] unified sort: cycle detected (sorted {} of {})", sorted, n);
            for (int i = 0; i < n; ++i)
                if (in_deg[i] > 0)
                    spdlog::critical("[Scheduler]   stuck: {} (in_deg={})", evals[i]->name(), in_deg[i]);
            std::_Exit(1);
        }

        // Group into waves.
        int num_w = 0;
        for (int i = 0; i < n; ++i)
            num_w = std::max(num_w, level[i] + 1);

        std::vector<std::vector<Component*>> waves(num_w);
        for (int i = 0; i < n; ++i)
            waves[level[i]].push_back(evals[i]);

        spdlog::info("[Scheduler] === unified wave plan ({} waves, {} components) ===", num_w, n);
        for (int w = 0; w < num_w; ++w) {
            spdlog::info("[Scheduler] wave {}: {} components", w, waves[w].size());
            for (auto* c : waves[w])
                spdlog::info("[Scheduler]   - {}", c->name());
        }

        // Store unified waves for execution (excluding fibers -- they resume separately).
        auto is_fiber = [&](Component* c) {
            for (int i = 0; i < fiber_count_; ++i)
                if (fibers_[i] == c) return true;
            return false;
        };
        unified_num_waves_ = 0;
        for (int w = 0; w < num_w; ++w) {
            int count = 0;
            for (auto* c : waves[w]) {
                if (is_fiber(c)) continue;
                unified_waves_[unified_num_waves_][count++] = c;
            }
            if (count > 0) {
                unified_wave_counts_[unified_num_waves_] = count;
                ++unified_num_waves_;
            }
        }
        unified_resolved_ = true;
        spdlog::info("[Scheduler] === unified execution plan ({} waves) ===", unified_num_waves_);
        for (int w = 0; w < unified_num_waves_; ++w) {
            spdlog::info("[Scheduler] exec wave {}: {} components", w, unified_wave_counts_[w]);
            for (int i = 0; i < unified_wave_counts_[w]; ++i)
                spdlog::info("[Scheduler]   - {}", unified_waves_[w][i]->name());
        }

        // Dump DOT with wave subgraph clustering.
        // Visuals are outside waves; fibers are already in evals.
        std::vector<Component*> all;
        for (int i = 0; i < visual_count_; ++i) all.push_back(visuals_[i]);
        for (auto* c : evals) all.push_back(c);
        int total = static_cast<int>(all.size());
        int eval_start = visual_count_;  // evals start here in 'all'

        // Build a map from component pointer to index in 'all'.
        auto idx_of = [&](Component* c) -> int {
            for (int i = 0; i < total; ++i)
                if (all[i] == c) return i;
            return -1;
        };

        FILE* f = std::fopen("unified_waves.dot", "w");
        if (!f) return;

        std::fprintf(f, "digraph unified_waves {\n");
        std::fprintf(f, "  rankdir=LR;\n");
        std::fprintf(f, "  node [shape=box fontname=\"Consolas\" fontsize=10 style=filled];\n");
        std::fprintf(f, "  edge [fontname=\"Consolas\" fontsize=8 color=\"#555555\"];\n");
        std::fprintf(f, "  graph [nodesep=0.3 ranksep=0.8 compound=true];\n\n");

        // Wave colors (cycle through a palette).
        const char* wave_colors[] = {
            "#e8f4f8", "#e8f8e8", "#f8f4e8", "#f8e8e8",
            "#e8e8f8", "#f4e8f8", "#e8f8f4", "#f8f8e8",
        };
        int ncolors = 8;

        // Non-wave components (visuals, fibers).
        for (int i = 0; i < eval_start; ++i) {
            auto* c = all[i];
            std::string label = c->description().empty()
                ? c->name() : c->description() + "\\n" + c->name();
            std::fprintf(f, "  n%d [label=\"%s\" fillcolor=\"#dddddd\"];\n",
                         i, label.c_str());
        }
        std::fprintf(f, "\n");

        // Wave subgraphs.
        for (int w = 0; w < num_w; ++w) {
            std::fprintf(f, "  subgraph cluster_wave%d {\n", w);
            std::fprintf(f, "    label=\"wave %d\";\n", w);
            std::fprintf(f, "    style=filled; color=\"%s\";\n", wave_colors[w % ncolors]);
            std::fprintf(f, "    fontname=\"Consolas\"; fontsize=11;\n");
            for (auto* c : waves[w]) {
                int idx = idx_of(c);
                std::string label = c->description().empty()
                    ? c->name() : c->description() + "\\n" + c->name();
                std::fprintf(f, "    n%d [label=\"%s\" fillcolor=\"white\"];\n",
                             idx, label.c_str());
            }
            std::fprintf(f, "  }\n\n");
        }

        // Edges:
        //   solid grey  = ordering edge (out -> in, must eval producer first)
        //   dashed orange = bidirectional bus (io -- shared, no ordering)
        //   dashed red    = async input (sampled on a future cycle, no ordering)
        struct PinInfo { int slot; const char* name; };
        std::vector<std::vector<PinInfo>> in_pins(total), async_pins(total),
                                          out_pins(total), io_pins(total);
        for (int i = 0; i < total; ++i) {
            auto* c = all[i];
            for (int s = 1; s < SignalPool::count; ++s) {
                const char* nm = SignalPool::names[s];
                if (!nm) continue;
                bool is_in    = (c->inputs()[s / 64]       >> (s % 64)) & 1;
                bool is_out   = (c->outputs()[s / 64]      >> (s % 64)) & 1;
                bool is_async = (c->async_inputs()[s / 64]  >> (s % 64)) & 1;
                if (is_in && is_out)  io_pins[i].push_back({s, nm});
                else if (is_async)    async_pins[i].push_back({s, nm});
                else if (is_in)       in_pins[i].push_back({s, nm});
                else if (is_out)      out_pins[i].push_back({s, nm});
            }
        }

        auto has_slot = [](const std::vector<PinInfo>& v, int slot) {
            for (auto& p : v) if (p.slot == slot) return true;
            return false;
        };

        for (int a = 0; a < total; ++a) {
            for (int b = 0; b < total; ++b) {
                if (a == b) continue;
                // out -> in (ordering edge)
                for (auto& op : out_pins[a])
                    if (has_slot(in_pins[b], op.slot))
                        std::fprintf(f, "  n%d -> n%d [label=\"%s\"];\n", a, b, op.name);
                // out -> async_in (cross-cycle, no ordering)
                for (auto& op : out_pins[a])
                    if (has_slot(async_pins[b], op.slot))
                        std::fprintf(f, "  n%d -> n%d [label=\"%s\" style=dashed color=\"#cc4444\"];\n", a, b, op.name);
                // out -> io
                for (auto& op : out_pins[a])
                    if (has_slot(io_pins[b], op.slot))
                        std::fprintf(f, "  n%d -> n%d [label=\"%s\"];\n", a, b, op.name);
                // io -> in
                for (auto& bp : io_pins[a])
                    if (has_slot(in_pins[b], bp.slot))
                        std::fprintf(f, "  n%d -> n%d [label=\"%s\" style=dashed color=\"#cc8800\"];\n", a, b, bp.name);
                // io -> async_in (cross-cycle, no ordering)
                for (auto& bp : io_pins[a])
                    if (has_slot(async_pins[b], bp.slot))
                        std::fprintf(f, "  n%d -> n%d [label=\"%s\" style=dashed color=\"#cc4444\"];\n", a, b, bp.name);
                // io <-> io (no ordering, show as bidirectional dashed)
                if (a < b)
                    for (auto& bp : io_pins[a])
                        if (has_slot(io_pins[b], bp.slot))
                            std::fprintf(f, "  n%d -> n%d [label=\"%s\" dir=both style=dashed color=\"#cc8800\"];\n", a, b, bp.name);
            }
        }

        std::fprintf(f, "}\n");
        std::fclose(f);
        spdlog::info("[Scheduler] wrote unified_waves.dot");

        std::string cmd = "\"C:/Program Files/Graphviz/bin/dot.exe\" -Tsvg unified_waves.dot -o unified_waves.svg 2>&1";
        if (std::system(cmd.c_str()) == 0)
            spdlog::info("[Scheduler] rendered unified_waves.svg");
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

    // Commit pending signals, then evaluate all non-fiber components.
    // Fibers are resumed separately in evaluate().
    void evaluate_no_wake(bool rising = true, bool falling = true) {
        SignalPool::commit();

        if (false && unified_resolved_) {
            // Unified DAG: one loop, commit between each wave.
            for (int w = 0; w < unified_num_waves_; ++w) {
                for (int i = 0; i < unified_wave_counts_[w]; ++i)
                    unified_waves_[w][i]->on_signal_change(rising, falling);
                SignalPool::commit();
            }
            return;
        }

        // Legacy 3-phase fallback.
        for (int i = 0; i < bus_ctrl_count_; ++i)
            bus_ctrls_[i]->on_signal_change(rising, falling);
        for (;;) {
            for (int i = 0; i < inline_count_; ++i)
                inlines_[i]->on_signal_change(rising, falling);
            if (!SignalPool::commit()) break;
        }
        if (resolved_) {
            for (int w = 0; w < num_waves_; ++w) {
                for (int i = 0; i < wave_counts_[w]; ++i)
                    waves_[w][i]->on_signal_change(rising, falling);
                SignalPool::commit();
            }
        } else {
            for (int i = 0; i < callback_count_; ++i)
                callbacks_[i]->on_signal_change(rising, falling);
        }
    }

private:
    bool half_cycle_ = false;
    bool resolved_ = false;
    bool unified_resolved_ = false;

    static constexpr int MAX_BUS_CTRLS = 4;
    static constexpr int MAX_INLINES = 32;
    static constexpr int MAX_CALLBACKS = 32;
    static constexpr int MAX_FIBERS = 256;
    static constexpr int MAX_WAVES = 16;
    static constexpr int MAX_UNIFIED = MAX_BUS_CTRLS + MAX_INLINES + MAX_CALLBACKS;

    BusControllerComponent* bus_ctrls_[MAX_BUS_CTRLS] = {};
    int bus_ctrl_count_ = 0;

    InlineComponent* inlines_[MAX_INLINES] = {};
    int inline_count_ = 0;

    CallbackComponent* callbacks_[MAX_CALLBACKS] = {};
    int callback_count_ = 0;

    // Resolved callback waves (populated by resolve(), legacy fallback).
    CallbackComponent* waves_[MAX_WAVES][MAX_CALLBACKS] = {};
    int wave_counts_[MAX_WAVES] = {};
    int num_waves_ = 0;

    // Unified DAG waves (populated by dump_unified_waves(), excludes fibers).
    Component* unified_waves_[MAX_WAVES][MAX_UNIFIED] = {};
    int unified_wave_counts_[MAX_WAVES] = {};
    int unified_num_waves_ = 0;

    FiberComponent* fibers_[MAX_FIBERS] = {};
    int fiber_count_ = 0;

    static constexpr int MAX_VISUALS = 8;
    Component* visuals_[MAX_VISUALS] = {};
    int visual_count_ = 0;
};

} // namespace bench
