#pragma once
#include "core/signal.h"
#include "core/callback_component.h"
#include "core/fiber_component.h"
#include <array>
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>

namespace bench {

// Central synchronous evaluator -- the beating heart of the simulation.
//
// Called by the 8284A at each CLK edge. Evaluates all components
// in topological wave order, then resumes fiber components cooperatively.
//
// Callback execution order is auto-resolved from pin declarations:
// resolve() builds a dependency DAG (A.outputs & B.inputs != 0 => A before B),
// topologically sorts into waves, and inserts a commit() between each wave.
class Scheduler {
public:
    void register_callback(CallbackComponent* cc) {
        assert(callback_count_ < MAX_CALLBACKS && "Scheduler: too many callbacks");
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

        //dump_dot("callback_graph");
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

    // Build per-permutation wave plans for all bidirectional pin configurations.
    // For N bidir blocks across all components, builds 2^N DAGs. At runtime,
    // evaluate_no_wake() checks each block's lambda to select the correct plan.
    void dump_unified_waves() {
        // Collect ALL evaluable components (fibers included for DAG, excluded from exec).
        std::vector<Component*> evals;
        for (int i = 0; i < fiber_count_;     ++i) evals.push_back(fibers_[i]);
        for (int i = 0; i < callback_count_;  ++i) evals.push_back(callbacks_[i]);
        const int n = static_cast<int>(evals.size());
        if (n == 0) return;

        constexpr int W = Component::SLOT_WORDS;

        // Collect all bidir blocks from all components.
        bidir_refs_.clear();
        for (auto* c : evals) {
            for (auto& block : c->bidir_blocks())
                bidir_refs_.push_back({c, &block});
        }

        const int num_bidir = static_cast<int>(bidir_refs_.size());

        // BidirDir bit flags: HiZ=1, Input=2, Output=4.
        // Each block's state occupies one base-3 digit in the permutation index,
        // mapping: digit 0 -> HiZ, digit 1 -> Input, digit 2 -> Output.
        // Total index space is 3^N, but we only build plans for valid
        // combinations (each block's digit must be in its `possible` mask).
        using BidirDir = Component::BidirDir;
        static constexpr BidirDir digit_to_dir[3] = {
            BidirDir::HiZ, BidirDir::Input, BidirDir::Output
        };

        int num_slots = 1;  // 3^N index space
        for (int i = 0; i < num_bidir; ++i) num_slots *= 3;

        auto perm_dir = [](int perm, int b) -> BidirDir {
            for (int i = 0; i < b; ++i) perm /= 3;
            return digit_to_dir[perm % 3];
        };

        // Check if a permutation is valid (all blocks in their possible set).
        auto perm_valid = [&](int perm) -> bool {
            for (int b = 0; b < num_bidir; ++b)
                if (!(bidir_refs_[b].block->possible & perm_dir(perm, b)))
                    return false;
            return true;
        };

        int num_valid = 0;
        for (int p = 0; p < num_slots; ++p)
            if (perm_valid(p)) ++num_valid;

        spdlog::info("[Scheduler] {} bidir blocks -> {} valid permutations (of {} slots)",
                     num_bidir, num_valid, num_slots);
        for (int i = 0; i < num_bidir; ++i)
            spdlog::info("[Scheduler]   block {}: {} (possible=0x{:x})", i,
                         bidir_refs_[i].comp->name(), uint8_t(bidir_refs_[i].block->possible));

        // Which component index owns each bidir block?
        bidir_comp_idx_.resize(num_bidir);
        for (int b = 0; b < num_bidir; ++b) {
            for (int i = 0; i < n; ++i) {
                if (evals[i] == bidir_refs_[b].comp) { bidir_comp_idx_[b] = i; break; }
            }
        }

        // Store evals list for on-demand solve_perm().
        evals_.assign(evals.begin(), evals.end());
        wave_plans_.clear();

        unified_resolved_ = true;

        // Dump DOT/SVG for each permutation.
        //dump_permutation_dots(evals, n);
    }

    // Dump one DOT/SVG per DAG permutation, showing wave clustering and edges.
    void dump_permutation_dots(const std::vector<Component*>& evals, int n) {
        constexpr int W = Component::SLOT_WORDS;

        // Gather visuals + evals into one flat list for node indexing.
        std::vector<Component*> all;
        for (int i = 0; i < visual_count_; ++i) all.push_back(visuals_[i]);
        for (auto* c : evals) all.push_back(c);
        int total = static_cast<int>(all.size());
        int eval_start = visual_count_;

        // Wave colors.
        const char* wave_colors[] = {
            "#e8f4f8", "#e8f8e8", "#f8f4e8", "#f8e8e8",
            "#e8e8f8", "#f4e8f8", "#e8f8f4", "#f8f8e8",
        };

        auto idx_of = [&](Component* c) -> int {
            for (int i = 0; i < total; ++i)
                if (all[i] == c) return i;
            return -1;
        };

        using BidirDir = Component::BidirDir;
        static constexpr BidirDir digit_to_dir[3] = {
            BidirDir::HiZ, BidirDir::Input, BidirDir::Output
        };
        auto perm_dir = [](int perm, int b) -> BidirDir {
            for (int i = 0; i < b; ++i) perm /= 3;
            return digit_to_dir[perm % 3];
        };
        auto dir_str = [](BidirDir d) -> const char* {
            switch (d) {
                case BidirDir::HiZ:    return "=HiZ";
                case BidirDir::Input:  return "=IN";
                case BidirDir::Output: return "=OUT";
            }
            return "=?";
        };

        // Build a bidir label string for a permutation.
        auto perm_label = [&](int perm) -> std::string {
            std::string s;
            for (int b = 0; b < static_cast<int>(bidir_refs_.size()); ++b) {
                if (!s.empty()) s += ", ";
                s += bidir_refs_[b].comp->name();
                s += dir_str(perm_dir(perm, b));
            }
            return s;
        };

        // Which 'all' index owns each bidir block?
        const int num_bidir = static_cast<int>(bidir_refs_.size());
        std::vector<int> bidir_all_idx(num_bidir);
        for (int b = 0; b < num_bidir; ++b)
            bidir_all_idx[b] = idx_of(bidir_refs_[b].comp);

        for (auto& [perm, plan] : wave_plans_) {
            std::string dot_path = "unified_waves_perm" + std::to_string(perm) + ".dot";
            std::string svg_path = "unified_waves_perm" + std::to_string(perm) + ".svg";
            FILE* f = std::fopen(dot_path.c_str(), "w");
            if (!f) continue;

            std::string title = "perm " + std::to_string(perm) + ": " + perm_label(perm);
            std::fprintf(f, "digraph unified_perm%d {\n", perm);
            std::fprintf(f, "  rankdir=LR;\n");
            std::fprintf(f, "  label=\"%s\";\n", title.c_str());
            std::fprintf(f, "  labelloc=t; fontsize=14; fontname=\"Consolas\";\n");
            std::fprintf(f, "  node [shape=box fontname=\"Consolas\" fontsize=10 style=filled];\n");
            std::fprintf(f, "  edge [fontname=\"Consolas\" fontsize=8 color=\"#555555\"];\n");
            std::fprintf(f, "  graph [nodesep=0.3 ranksep=0.8 compound=true];\n\n");

            // Non-wave components (visuals + fibers).
            for (int i = 0; i < total; ++i) {
                auto* c = all[i];
                bool in_wave = false;
                for (auto& wave : plan.waves)
                    for (auto* wc : wave)
                        if (wc == c) { in_wave = true; break; }
                if (in_wave) continue;
                std::string label = c->description().empty()
                    ? c->name() : c->description() + "\\n" + c->name();
                std::fprintf(f, "  n%d [label=\"%s\" fillcolor=\"#dddddd\"];\n",
                             i, label.c_str());
            }
            std::fprintf(f, "\n");

            // Wave subgraphs.
            for (int w = 0; w < static_cast<int>(plan.waves.size()); ++w) {
                std::fprintf(f, "  subgraph cluster_wave%d {\n", w);
                std::fprintf(f, "    label=\"wave %d\";\n", w);
                std::fprintf(f, "    style=filled; color=\"%s\";\n", wave_colors[w % 8]);
                std::fprintf(f, "    fontname=\"Consolas\"; fontsize=11;\n");
                for (auto* c : plan.waves[w]) {
                    int idx = idx_of(c);
                    std::string label = c->description().empty()
                        ? c->name() : c->description() + "\\n" + c->name();
                    std::fprintf(f, "    n%d [label=\"%s\" fillcolor=\"white\"];\n",
                                 idx, label.c_str());
                }
                std::fprintf(f, "  }\n\n");
            }

            // Compute per-component effective outputs/inputs for this permutation.
            // Bidir pins are resolved to directed based on the permutation bit.
            std::vector<std::array<uint64_t, W>> eff_out(total), eff_in(total);
            for (int i = 0; i < total; ++i) {
                auto* c = all[i];
                for (int w = 0; w < W; ++w) {
                    eff_out[i][w] = c->outputs()[w] & ~c->inputs()[w];
                    eff_in[i][w]  = c->inputs()[w]  & ~c->async_inputs()[w];
                }
            }
            for (int b = 0; b < num_bidir; ++b) {
                int ci = bidir_all_idx[b];
                if (ci < 0) continue;
                BidirDir dir = perm_dir(perm, b);
                for (int w = 0; w < W; ++w) {
                    uint64_t om = bidir_refs_[b].block->out_mask[w];
                    uint64_t im = bidir_refs_[b].block->in_mask[w];
                    switch (dir) {
                        case BidirDir::Output:
                            eff_out[ci][w] |= om;  eff_in[ci][w] &= ~om;
                            eff_out[ci][w] &= ~im;
                            break;
                        case BidirDir::Input:
                            eff_out[ci][w] &= ~om;
                            eff_out[ci][w] |= im;  eff_in[ci][w] &= ~im;
                            break;
                        case BidirDir::HiZ:
                            eff_out[ci][w] &= ~om;  eff_in[ci][w] &= ~om;
                            eff_out[ci][w] &= ~im;  eff_in[ci][w] &= ~im;
                            break;
                    }
                }
            }

            // Edges: directed based on this permutation's effective pins.
            // Ordering edges (solid grey), async edges (dashed red).
            for (int a = 0; a < total; ++a) {
                for (int b2 = 0; b2 < total; ++b2) {
                    if (a == b2) continue;
                    // Collect signal names for ordering edges and async edges.
                    for (int s = 1; s < SignalPool::count; ++s) {
                        const char* nm = SignalPool::names[s];
                        if (!nm) continue;
                        int w = s / 64;
                        uint64_t bit = uint64_t(1) << (s % 64);
                        bool a_drives = (eff_out[a][w] & bit) != 0;
                        if (!a_drives) continue;
                        bool b_reads  = (eff_in[b2][w] & bit) != 0;
                        bool b_async  = (all[b2]->async_inputs()[w] & bit) != 0;
                        if (b_reads)
                            std::fprintf(f, "  n%d -> n%d [label=\"%s\"];\n", a, b2, nm);
                        else if (b_async)
                            std::fprintf(f, "  n%d -> n%d [label=\"%s\" style=dashed color=\"#cc4444\"];\n", a, b2, nm);
                    }
                }
            }

            std::fprintf(f, "}\n");
            std::fclose(f);
            spdlog::info("[Scheduler] wrote {}", dot_path);

            std::string cmd = "\"C:/Program Files/Graphviz/bin/dot.exe\" -Tsvg "
                              + dot_path + " -o " + svg_path + " 2>&1";
            if (std::system(cmd.c_str()) == 0) {
                spdlog::info("[Scheduler] rendered {}", svg_path);
                std::remove(dot_path.c_str());
            } else {
                spdlog::warn("[Scheduler] dot not found -- SVG not rendered");
            }
        }
    }

    // Render SVG for every DAG permutation encountered at runtime.
    // Deletes intermediate .dot files.
    void dump_permutation_svgs() {
        dump_permutation_dots(evals_, static_cast<int>(evals_.size()));
    }

    // Evaluate all components in topological wave order.
    void evaluate(Fiber caller = nullptr) {
        // Select DAG permutation by checking bidir block lambdas.
        // Map BidirDir bit flags to base-3 digits: HiZ(1)->0, Input(2)->1, Output(4)->2.
        // Lambdas may read pin levels -- suspend validation during selection.
#ifdef BENCH_PIN_VALIDATION
        SignalPool::end_component();
#endif
        static constexpr int dir_to_digit[] = {-1, 0, 1, -1, 2};  // indexed by uint8_t(BidirDir)
        int perm = 0, mul = 1;
        for (int i = 0; i < static_cast<int>(bidir_refs_.size()); ++i) {
            perm += dir_to_digit[uint8_t(bidir_refs_[i].block->direction())] * mul;
            mul *= 3;
        }

        auto it = wave_plans_.find(perm);
        if (it == wave_plans_.end())
            it = wave_plans_.emplace(perm, solve_perm(perm)).first;
        for (auto& wave : it->second.waves) {
            for (auto* c : wave) {
#ifdef BENCH_PIN_VALIDATION
                SignalPool::begin_component(c);
#endif
                c->on_signal_change(caller);
#ifdef BENCH_PIN_VALIDATION
                SignalPool::end_component();
#endif
            }
        }
    }

private:
    bool resolved_ = false;
    bool unified_resolved_ = false;

    static constexpr int MAX_CALLBACKS = 64;
    static constexpr int MAX_FIBERS = 256;
    static constexpr int MAX_WAVES = 16;

    CallbackComponent* callbacks_[MAX_CALLBACKS] = {};
    int callback_count_ = 0;

    // Resolved callback waves (populated by resolve(), legacy fallback).
    CallbackComponent* waves_[MAX_WAVES][MAX_CALLBACKS] = {};
    int wave_counts_[MAX_WAVES] = {};
    int num_waves_ = 0;

    // Per-permutation wave plans (populated by dump_unified_waves()).
    struct WavePlan {
        std::vector<std::vector<Component*>> waves;
    };
    std::unordered_map<int, WavePlan> wave_plans_;

    // Bidir block references for runtime DAG selection.
    struct BidirRef {
        Component* comp;
        const Component::BidirBlock* block;
    };
    std::vector<BidirRef> bidir_refs_;
    std::vector<Component*> evals_;
    std::vector<int> bidir_comp_idx_;

    WavePlan solve_perm(int perm) {
        constexpr int W = Component::SLOT_WORDS;
        using BidirDir = Component::BidirDir;
        static constexpr BidirDir digit_to_dir[3] = {
            BidirDir::HiZ, BidirDir::Input, BidirDir::Output
        };
        auto perm_dir = [](int p, int b) -> BidirDir {
            for (int i = 0; i < b; ++i) p /= 3;
            return digit_to_dir[p % 3];
        };

        const int n = static_cast<int>(evals_.size());
        const int num_bidir = static_cast<int>(bidir_refs_.size());

        std::vector<std::array<uint64_t, W>> eff_out(n), eff_in(n);
        for (int i = 0; i < n; ++i)
            for (int w = 0; w < W; ++w) {
                eff_out[i][w] = evals_[i]->outputs()[w] & ~evals_[i]->inputs()[w];
                eff_in[i][w]  = evals_[i]->inputs()[w]  & ~evals_[i]->async_inputs()[w];
            }

        for (int b = 0; b < num_bidir; ++b) {
            int ci = bidir_comp_idx_[b];
            BidirDir dir = perm_dir(perm, b);
            for (int w = 0; w < W; ++w) {
                uint64_t om = bidir_refs_[b].block->out_mask[w];
                uint64_t im = bidir_refs_[b].block->in_mask[w];
                switch (dir) {
                    case BidirDir::Output:
                        eff_out[ci][w] |= om;  eff_in[ci][w] &= ~om;
                        eff_out[ci][w] &= ~im;
                        break;
                    case BidirDir::Input:
                        eff_out[ci][w] &= ~om;
                        eff_out[ci][w] |= im;  eff_in[ci][w] &= ~im;
                        break;
                    case BidirDir::HiZ:
                        eff_out[ci][w] &= ~om;  eff_in[ci][w] &= ~om;
                        eff_out[ci][w] &= ~im;  eff_in[ci][w] &= ~im;
                        break;
                }
            }
        }

        std::vector<std::vector<bool>> depends(n, std::vector<bool>(n, false));
        for (int a = 0; a < n; ++a)
            for (int b2 = 0; b2 < n; ++b2) {
                if (a == b2) continue;
                for (int w = 0; w < W; ++w)
                    if (eff_out[a][w] & eff_in[b2][w]) {
                        depends[b2][a] = true;
                        break;
                    }
            }

        std::vector<int> in_deg(n, 0), level(n, 0);
        for (int b2 = 0; b2 < n; ++b2)
            for (int a = 0; a < n; ++a)
                if (depends[b2][a]) ++in_deg[b2];

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
            spdlog::critical("[Scheduler] solve_perm {}: cycle in DAG (sorted {} of {})", perm, sorted, n);
            for (int i = 0; i < n; ++i)
                if (in_deg[i] > 0)
                    spdlog::critical("[Scheduler]   stuck: {} (in_deg={})", evals_[i]->name(), in_deg[i]);
            std::_Exit(1);
        }

        int num_w = 0;
        for (int i = 0; i < n; ++i)
            num_w = std::max(num_w, level[i] + 1);

        WavePlan plan;
        for (int w = 0; w < num_w; ++w) {
            std::vector<Component*> wave;
            for (int i = 0; i < n; ++i)
                if (level[i] == w)
                    wave.push_back(evals_[i]);
            if (!wave.empty())
                plan.waves.push_back(std::move(wave));
        }

        spdlog::info("[Scheduler] solved perm {} ({} waves):", perm, plan.waves.size());
        for (int w = 0; w < static_cast<int>(plan.waves.size()); ++w) {
            std::string names;
            for (auto* c : plan.waves[w]) {
                if (!names.empty()) names += ", ";
                names += c->name();
            }
            spdlog::info("[Scheduler]   wave {}: {}", w, names);
        }

        return plan;
    }

    FiberComponent* fibers_[MAX_FIBERS] = {};
    int fiber_count_ = 0;

    static constexpr int MAX_VISUALS = 8;
    Component* visuals_[MAX_VISUALS] = {};
    int visual_count_ = 0;
};

} // namespace bench
