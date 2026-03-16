#pragma once
#include "core/signal.h"
#include "core/callback_component.h"
#include "core/fiber_component.h"
#include <array>
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <tuple>
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
    // A group of components that can be skipped when inactive.
    // is_active() is called once per evaluate(); if false, all components
    // in the group are skipped for that cycle.
    struct ComponentGroup {
        std::string name;
        std::function<bool()> is_active;
    };

    static constexpr int MAX_GROUPS = 1;

    int register_group(std::string name, std::function<bool()> is_active_fn) {
        int id = num_groups_++;
        assert(id < MAX_GROUPS);
        groups_[id] = {std::move(name), std::move(is_active_fn)};
        return id;
    }

    void register_callback(CallbackComponent* cc, int group_id = -1) {
        callbacks_.push_back(cc);
        callback_group_.push_back(group_id);
        cc->group_id_ = group_id;
    }

    void register_fiber(FiberComponent* fc) {
        fibers_.push_back(fc);
    }

    // Register a component for visualization only (e.g. threaded 8284A).
    void register_visual(Component* c) {
        visuals_.push_back(c);
    }

    // Power on/off all registered components (called by PSU on clock thread).
    // Order: callbacks first, then fibers (mirrors the old manual sequence).
    void power_on_all() {
        for (auto* cc : callbacks_) cc->power_on();
        for (auto* fc : fibers_)    fc->power_on();
    }
    void power_off_all() {
        // Fibers first (CPU), then callbacks (reverse of power-on).
        for (auto* fc : fibers_)    fc->power_off();
        for (auto* cc : callbacks_) cc->power_off();
    }

    // Build the callback dependency graph and topologically sort into waves.
    // Call once after all callbacks are registered, before the first evaluate().
    void resolve() {
        const int n = static_cast<int>(callbacks_.size());
        if (n == 0) { resolved_ = true; return; }

        constexpr int W = Component::SLOT_WORDS;

        // Build adjacency: depends[b][a] = true means b depends on a.
        // Uses effective outputs (output & ~input) to exclude bidirectional pins,
        // and effective inputs (input & ~async) to exclude cross-cycle signals.
        std::vector<std::vector<bool>> depends(n, std::vector<bool>(n, false));
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
        std::vector<int> in_deg(n, 0);
        std::vector<int> level(n, 0);
        for (int b = 0; b < n; ++b)
            for (int a = 0; a < n; ++a)
                if (depends[b][a]) ++in_deg[b];

        std::vector<int> queue;
        queue.reserve(n);
        for (int i = 0; i < n; ++i)
            if (in_deg[i] == 0) queue.push_back(i);

        int sorted = 0;
        for (int front = 0; front < static_cast<int>(queue.size()); ++front) {
            int u = queue[front];
            ++sorted;
            for (int v = 0; v < n; ++v) {
                if (!depends[v][u]) continue;
                level[v] = std::max(level[v], level[u] + 1);
                if (--in_deg[v] == 0) queue.push_back(v);
            }
        }
        if (sorted != n) {
            spdlog::critical("[Scheduler] cycle in callback dependencies (sorted {} of {})", sorted, n);
            for (int i = 0; i < n; ++i) {
                if (in_deg[i] <= 0) continue;
                spdlog::critical("[Scheduler]   stuck: {} (in_deg={})", callbacks_[i]->name(), in_deg[i]);
                for (int j = 0; j < n; ++j) {
                    if (!depends[i][j] || in_deg[j] <= 0) continue;
                    for (int s = 1; s < SignalPool::count; ++s) {
                        int w2 = s / 64;
                        uint64_t bit = uint64_t(1) << (s % 64);
                        uint64_t eff_out = callbacks_[j]->outputs()[w2] & ~callbacks_[j]->inputs()[w2];
                        uint64_t eff_in  = callbacks_[i]->inputs()[w2]  & ~callbacks_[i]->async_inputs()[w2];
                        if ((eff_out & bit) && (eff_in & bit))
                            spdlog::critical("[Scheduler]     <- {} via signal #{}", callbacks_[j]->name(), s);
                    }
                }
            }
            std::_Exit(1);
        }

        // Populate waves.
        int num_waves = 0;
        for (int i = 0; i < n; ++i)
            num_waves = std::max(num_waves, level[i] + 1);

        waves_.resize(num_waves);
        for (auto& w : waves_) w.clear();

        for (int i = 0; i < n; ++i)
            waves_[level[i]].push_back(callbacks_[i]);

        for (int w = 0; w < num_waves; ++w) {
            spdlog::info("[Scheduler] wave {}: {} callbacks", w, waves_[w].size());
            for (auto* c : waves_[w])
                spdlog::info("[Scheduler]   - {}", c->name());
        }

        dump_unified_waves();
        resolved_ = true;
    }

    // Build per-permutation wave plans for all bidirectional pin configurations.
    // For N bidir blocks across all components, builds 2^N DAGs. At runtime,
    // evaluate_no_wake() checks each block's lambda to select the correct plan.
    void dump_unified_waves() {
        // Collect ALL evaluable components (fibers included for DAG, excluded from exec).
        std::vector<Component*> evals;
        for (auto* fc : fibers_)    evals.push_back(fc);
        for (auto* cc : callbacks_) evals.push_back(cc);
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
        for (auto* v : visuals_) all.push_back(v);
        for (auto* c : evals) all.push_back(c);
        int total = static_cast<int>(all.size());
        int eval_start = static_cast<int>(visuals_.size());

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
            std::string svg_path = "unified_waves_perm" + std::to_string(perm) + ".svg";
            FILE* f = std::fopen(svg_path.c_str(), "w");
            if (!f) continue;

            // Layout constants.
            constexpr int BOX_W = 130, BOX_H = 40, PAD = 30, GAP_X = 20, GAP_Y = 80;
            constexpr int WAVE_PAD = 12, TITLE_H = 30;

            // Assign each component a (wave, slot) position.
            struct NodePos { int x, y, w, h; int wave; };
            std::vector<NodePos> pos(total);
            std::vector<bool> in_wave_flag(total, false);
            for (auto& wave : plan.waves)
                for (auto* wc : wave)
                    in_wave_flag[idx_of(wc)] = true;

            // Non-wave components (visuals).
            std::vector<int> non_wave;
            for (int i = 0; i < total; ++i)
                if (!in_wave_flag[i]) non_wave.push_back(i);

            // Max wave row width (inactive components excluded).
            int max_cols = 0;
            for (auto& wave : plan.waves)
                max_cols = std::max(max_cols, static_cast<int>(wave.size()));

            int wave_area_w = PAD * 2 + max_cols * (BOX_W + GAP_X) - GAP_X + WAVE_PAD * 2;
            if (wave_area_w < 600) wave_area_w = 600;
            int cur_y = PAD + TITLE_H;

            // Position wave rows.
            struct WaveRect { int x, y, w, h; };
            std::vector<WaveRect> wave_rects;
            for (int w = 0; w < static_cast<int>(plan.waves.size()); ++w) {
                int nc = static_cast<int>(plan.waves[w].size());
                int row_w = nc * (BOX_W + GAP_X) - GAP_X;
                int wave_w = row_w + WAVE_PAD * 2;
                int x0 = (wave_area_w - wave_w) / 2;
                int wave_h = BOX_H + WAVE_PAD * 2 + 16;
                wave_rects.push_back({x0, cur_y, wave_w, wave_h});
                int bx = x0 + WAVE_PAD;
                for (int i = 0; i < nc; ++i) {
                    int idx = idx_of(plan.waves[w][i]);
                    pos[idx] = {bx + i * (BOX_W + GAP_X), cur_y + WAVE_PAD + 16, BOX_W, BOX_H, w};
                }
                cur_y += wave_h + GAP_Y;
            }
            int wave_area_h = cur_y + PAD;

            // Position inactive components in a box to the lower right.
            constexpr int INACT_COL_W = 140, INACT_ROW_H = 50, INACT_PAD = 12;
            constexpr int INACT_COLS = 3;
            int inact_rows = (static_cast<int>(non_wave.size()) + INACT_COLS - 1) / INACT_COLS;
            int inact_box_w = INACT_COLS * INACT_COL_W + INACT_PAD * 2;
            int inact_box_h = inact_rows * INACT_ROW_H + INACT_PAD * 2 + 16;
            int inact_x0 = wave_area_w + PAD;
            int inact_y0 = wave_area_h - inact_box_h - PAD;
            if (inact_y0 < PAD + TITLE_H) inact_y0 = PAD + TITLE_H;

            for (int i = 0; i < static_cast<int>(non_wave.size()); ++i) {
                int col = i % INACT_COLS, row = i / INACT_COLS;
                int bx = inact_x0 + INACT_PAD + col * INACT_COL_W + (INACT_COL_W - BOX_W) / 2;
                int by = inact_y0 + INACT_PAD + 16 + row * INACT_ROW_H;
                pos[non_wave[i]] = {bx, by, BOX_W, BOX_H, -1};
            }

            int svg_w = non_wave.empty() ? wave_area_w : inact_x0 + inact_box_w + PAD;
            int svg_h = std::max(wave_area_h, inact_y0 + inact_box_h + PAD);

            // Compute effective outputs/inputs for this permutation.
            std::vector<std::array<uint64_t, W>> eff_out(total), eff_in(total);
            for (int i = 0; i < total; ++i) {
                auto* c = all[i];
                for (int w2 = 0; w2 < W; ++w2) {
                    eff_out[i][w2] = c->outputs()[w2] & ~c->inputs()[w2];
                    // Include async_inputs so CLK->CPU etc. appear as edges.
                    eff_in[i][w2]  = c->inputs()[w2] | c->async_inputs()[w2];
                }
            }
            for (int b = 0; b < num_bidir; ++b) {
                int ci = bidir_all_idx[b];
                if (ci < 0) continue;
                BidirDir dir = perm_dir(perm, b);
                for (int w2 = 0; w2 < W; ++w2) {
                    uint64_t om = bidir_refs_[b].block->out_mask[w2];
                    uint64_t im = bidir_refs_[b].block->in_mask[w2];
                    switch (dir) {
                        case BidirDir::Output:
                            eff_out[ci][w2] |= om;  eff_in[ci][w2] &= ~om;
                            eff_out[ci][w2] &= ~im;
                            break;
                        case BidirDir::Input:
                            eff_out[ci][w2] &= ~om;
                            eff_out[ci][w2] |= im;  eff_in[ci][w2] &= ~im;
                            break;
                        case BidirDir::HiZ:
                            eff_out[ci][w2] &= ~om;  eff_in[ci][w2] &= ~om;
                            eff_out[ci][w2] &= ~im;  eff_in[ci][w2] &= ~im;
                            break;
                    }
                }
            }

            // Collapse edges by (src, dst). Distinguish sync vs async signals.
            struct EdgeInfo {
                std::vector<std::string> sync_names;
                std::vector<std::string> async_names;
            };
            std::map<std::pair<int,int>, EdgeInfo> edges;
            for (int a = 0; a < total; ++a) {
                if (!in_wave_flag[a]) continue;  // skip inactive sources
                for (int b2 = 0; b2 < total; ++b2) {
                    if (a == b2 || !in_wave_flag[b2]) continue;  // skip inactive targets
                    for (int s = 1; s < SignalPool::count; ++s) {
                        const char* nm = SignalPool::names[s];
                        if (!nm) continue;
                        int w2 = s / 64;
                        uint64_t bit = uint64_t(1) << (s % 64);
                        if (!(eff_out[a][w2] & bit)) continue;
                        bool b_sync  = (all[b2]->inputs()[w2] & bit) != 0;
                        bool b_async = (all[b2]->async_inputs()[w2] & bit) != 0;
                        if (b_sync)
                            edges[{a, b2}].sync_names.push_back(nm);
                        else if (b_async)
                            edges[{a, b2}].async_names.push_back(nm);
                    }
                }
            }

            // Build compact label: count signals, abbreviate long lists.
            auto compact_label = [](const std::vector<std::string>& names) -> std::string {
                if (names.empty()) return "";
                if (names.size() <= 3) {
                    std::string s;
                    for (auto& n : names) { if (!s.empty()) s += ", "; s += n; }
                    return s;
                }
                return names.front() + " ... " + names.back()
                     + " (" + std::to_string(names.size()) + ")";
            };

            // --- Emit SVG ---
            std::fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            std::fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" "
                            "viewBox=\"0 0 %d %d\">\n", svg_w, svg_h, svg_w, svg_h);
            // White background.
            std::fprintf(f, "<rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");
            std::fprintf(f, "<style>\n");
            std::fprintf(f, "  text { font-family: Consolas, monospace; }\n");
            std::fprintf(f, "  .title { font-size: 13px; font-weight: bold; fill: #222; }\n");
            std::fprintf(f, "  .wave-label { font-size: 10px; fill: #666; }\n");
            std::fprintf(f, "  .node-desc { font-size: 8px; fill: #666; text-anchor: middle; }\n");
            std::fprintf(f, "  .node-name { font-size: 9px; font-weight: bold; text-anchor: middle; }\n");
            std::fprintf(f, "  .edge-label { font-size: 7px; fill: #777; }\n");
            std::fprintf(f, "</style>\n");
            std::fprintf(f, "<defs>\n");
            std::fprintf(f, "  <marker id=\"ah\" markerWidth=\"6\" markerHeight=\"4\" "
                            "refX=\"6\" refY=\"2\" orient=\"auto\" markerUnits=\"strokeWidth\">"
                            "<polygon points=\"0 0, 6 2, 0 4\" fill=\"context-stroke\"/></marker>\n");
            std::fprintf(f, "</defs>\n");

            // Title: perm number on first line, bidir states wrapped on second.
            std::fprintf(f, "<text x=\"%d\" y=\"%d\" class=\"title\" text-anchor=\"middle\">"
                            "perm %d</text>\n", svg_w / 2, PAD + 4, perm);
            std::string plabel = perm_label(perm);
            std::fprintf(f, "<text x=\"%d\" y=\"%d\" text-anchor=\"middle\" "
                            "font-size=\"9\" font-family=\"Consolas\" fill=\"#555\">%s</text>\n",
                         svg_w / 2, PAD + 18, plabel.c_str());

            // Wave backgrounds.
            for (int w = 0; w < static_cast<int>(wave_rects.size()); ++w) {
                auto& r = wave_rects[w];
                std::fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
                                "rx=\"6\" fill=\"%s\" stroke=\"#bbb\"/>\n",
                             r.x, r.y, r.w, r.h, wave_colors[w % 8]);
                std::fprintf(f, "<text x=\"%d\" y=\"%d\" class=\"wave-label\">wave %d</text>\n",
                             r.x + 6, r.y + 12, w);
            }

            // Inactive box.
            if (!non_wave.empty()) {
                std::fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
                                "rx=\"6\" fill=\"#f5f5f5\" stroke=\"#ccc\" stroke-dasharray=\"4,3\"/>\n",
                             inact_x0, inact_y0, inact_box_w, inact_box_h);
                std::fprintf(f, "<text x=\"%d\" y=\"%d\" class=\"wave-label\" fill=\"#999\">inactive</text>\n",
                             inact_x0 + 6, inact_y0 + 12);
            }

            // Edges -- spread attachment points across bottom/top of boxes.
            // For each node, count outgoing and incoming edge slots.
            std::map<int, int> out_slot_count, in_slot_count;
            std::map<int, int> out_slot_next, in_slot_next;
            for (auto& [key, info] : edges) {
                out_slot_count[key.first]++;
                in_slot_count[key.second]++;
            }
            for (auto& [id, cnt] : out_slot_count) out_slot_next[id] = 0;
            for (auto& [id, cnt] : in_slot_count)  in_slot_next[id] = 0;

            // Color edges by wave gap: adjacent=light, skip=darker.
            const char* gap_colors[] = {"#99bbdd", "#88aa66", "#cc9944", "#cc6666", "#9966aa", "#666666"};

            for (auto& [key, info] : edges) {
                int src = key.first, dst = key.second;
                bool is_async = info.sync_names.empty();

                // Spread x attachment across bottom/top of boxes.
                int src_cnt = out_slot_count[src];
                int src_idx = out_slot_next[src]++;
                int dst_cnt = in_slot_count[dst];
                int dst_idx = in_slot_next[dst]++;
                int x1 = pos[src].x + (BOX_W * (src_idx + 1)) / (src_cnt + 1);
                int y1 = pos[src].y + BOX_H;
                int x2 = pos[dst].x + (BOX_W * (dst_idx + 1)) / (dst_cnt + 1);
                int y2 = pos[dst].y;

                // Color by wave distance (async always red).
                int wave_gap = std::abs(pos[dst].wave - pos[src].wave);
                if (pos[src].wave < 0 || pos[dst].wave < 0) wave_gap = 1;
                const char* color = is_async ? "#cc4444"
                    : gap_colors[std::min(wave_gap, 5)];
                const char* dash = is_async ? " stroke-dasharray=\"4,3\"" : "";
                const char* marker = "ah";

                // Build full signal list for tooltip.
                std::string tip;
                for (auto& n : info.sync_names) { if (!tip.empty()) tip += ", "; tip += n; }
                if (!info.async_names.empty()) {
                    if (!tip.empty()) tip += " + ";
                    for (auto& n : info.async_names) { if (!tip.empty()) tip += ", "; tip += n; }
                }
                int nsigs = static_cast<int>(info.sync_names.size() + info.async_names.size());

                std::fprintf(f, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
                                "stroke=\"%s\" stroke-width=\"%.1f\" opacity=\"0.5\"%s "
                                "marker-end=\"url(#%s)\">"
                                "<title>%s</title></line>\n",
                             x1, y1, x2, y2, color,
                             nsigs > 4 ? 2.0 : 1.0,
                             dash, marker, tip.c_str());

                // Compact visible label -- only show for bundles with <= 3 signals.
                if (nsigs <= 3) {
                    std::string label = compact_label(
                        info.sync_names.empty() ? info.async_names : info.sync_names);
                    int mx = (x1 + x2) / 2 + 4, my = (y1 + y2) / 2 - 2;
                    std::fprintf(f, "<text x=\"%d\" y=\"%d\" class=\"edge-label\">%s</text>\n",
                                 mx, my, label.c_str());
                }
            }

            // Node boxes (drawn on top of edges).
            for (int i = 0; i < total; ++i) {
                auto* c = all[i];
                auto& p = pos[i];
                const char* fill = (p.wave >= 0) ? "white" : "#eee";
                std::fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
                                "rx=\"4\" fill=\"%s\" stroke=\"#444\" stroke-width=\"1\"/>\n",
                             p.x, p.y, p.w, p.h, fill);
                std::string desc = c->description();
                if (!desc.empty())
                    std::fprintf(f, "<text x=\"%d\" y=\"%d\" class=\"node-desc\">%s</text>\n",
                                 p.x + BOX_W / 2, p.y + 15, desc.c_str());
                std::fprintf(f, "<text x=\"%d\" y=\"%d\" class=\"node-name\">%s</text>\n",
                             p.x + BOX_W / 2, p.y + (desc.empty() ? 24 : 30), c->name().c_str());
            }

            std::fprintf(f, "</svg>\n");
            std::fclose(f);
            spdlog::info("[Scheduler] wrote {}", svg_path);
        }
    }

    // Render SVG for every DAG permutation encountered at runtime.
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

        // Fold group active/inactive state into permutation key.
        // Group bits sit above the bidir base-3 digits.
        int group_bits = 0;
        for (int g = 0; g < MAX_GROUPS; ++g)
            if (groups_[g].is_active())
                group_bits |= (1 << g);
        perm += group_bits * mul;

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

    std::vector<CallbackComponent*> callbacks_;
    std::vector<int> callback_group_;  // group id per callback (-1 = none)

    // Component groups (skippable subsystems).
    std::array<ComponentGroup, MAX_GROUPS> groups_;
    int num_groups_ = 0;

    // Resolved callback waves (populated by resolve(), used for diagnostics).
    std::vector<std::vector<CallbackComponent*>> waves_;

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

        // Extract group bits from the top of the permutation key.
        int bidir_space = 1;
        for (int i = 0; i < static_cast<int>(bidir_refs_.size()); ++i) bidir_space *= 3;
        int group_bits = (bidir_space > 0) ? perm / bidir_space : 0;

        // Filter out components belonging to inactive groups.
        std::vector<Component*> active_evals;
        active_evals.reserve(evals_.size());
        for (auto* c : evals_) {
            int gid = c->group_id_;
            if (gid >= 0 && !(group_bits & (1 << gid))) continue;
            active_evals.push_back(c);
        }

        // Remap bidir block indices to filtered component list.
        const int num_bidir = static_cast<int>(bidir_refs_.size());
        std::vector<int> bidir_active_idx(num_bidir, -1);
        for (int b = 0; b < num_bidir; ++b) {
            for (int i = 0; i < static_cast<int>(active_evals.size()); ++i) {
                if (active_evals[i] == bidir_refs_[b].comp) { bidir_active_idx[b] = i; break; }
            }
        }

        const int n = static_cast<int>(active_evals.size());

        std::vector<std::array<uint64_t, W>> eff_out(n), eff_in(n);
        for (int i = 0; i < n; ++i)
            for (int w = 0; w < W; ++w) {
                eff_out[i][w] = active_evals[i]->outputs()[w] & ~active_evals[i]->inputs()[w];
                eff_in[i][w]  = active_evals[i]->inputs()[w]  & ~active_evals[i]->async_inputs()[w];
            }

        for (int b = 0; b < num_bidir; ++b) {
            int ci = bidir_active_idx[b];
            if (ci < 0) continue;  // bidir block's component excluded
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
            for (int i = 0; i < n; ++i) {
                if (in_deg[i] <= 0) continue;
                spdlog::critical("[Scheduler]   stuck: {} (in_deg={})", active_evals[i]->name(), in_deg[i]);
                for (int j = 0; j < n; ++j) {
                    if (!depends[i][j]) continue;
                    // Find which signal creates this edge.
                    for (int s = 1; s < SignalPool::count; ++s) {
                        int w = s / 64;
                        uint64_t bit = uint64_t(1) << (s % 64);
                        if ((eff_out[j][w] & bit) && (eff_in[i][w] & bit)) {
                            const char* nm = SignalPool::names[s];
                            spdlog::critical("[Scheduler]     <- {} via {}", active_evals[j]->name(), nm ? nm : "?");
                        }
                    }
                }
            }
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
                    wave.push_back(active_evals[i]);
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

    std::vector<FiberComponent*> fibers_;
    std::vector<Component*> visuals_;
};

} // namespace bench
