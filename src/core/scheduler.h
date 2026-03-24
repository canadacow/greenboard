#pragma once
#include "core/signal.h"
#include "core/callback_component.h"
#include "core/fiber_component.h"
#include "ic/ic_8088.h"
#include <array>
#include <cassert>
#include <cinttypes>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <intrin.h>
#include <spdlog/spdlog.h>

#define WAVE_DEBUGS

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
        callbacks_.push_back(cc);
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

    // Initialize the per-perm DAG infrastructure. Static callback ordering
    // was removed -- the per-perm DAG handles all dependency ordering at runtime.
    void resolve() { dump_unified_waves(); }

    // Build per-permutation wave plans for all bidirectional pin configurations.
    // For N bidir blocks across all components, builds 2^N DAGs. At runtime,
    // evaluate_no_wake() checks each block's lambda to select the correct plan.
    static inline const std::string WAVE_OUTPUT_DIR = std::string(PROJECT_ROOT) + "/wave_output";

    void dump_unified_waves() {
        // Clean and recreate wave output directory.
        std::filesystem::remove_all(WAVE_OUTPUT_DIR);
        std::filesystem::create_directories(WAVE_OUTPUT_DIR);

        // Collect ALL evaluable components (fibers + callbacks both participate in DAG and exec).
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
    }

    // Dump one DOT/SVG per DAG permutation, same style as dump_cycle_dot.
    void dump_permutation_dots(const std::vector<Component*>& evals, int n) {
        constexpr int W = Component::SLOT_WORDS;

        using BidirDir = Component::BidirDir;
        static constexpr BidirDir digit_to_dir[3] = {
            BidirDir::HiZ, BidirDir::Input, BidirDir::Output
        };
        auto perm_dir = [](uint64_t p, int b) -> BidirDir {
            for (int i = 0; i < b; ++i) p /= 3;
            return digit_to_dir[p % 3];
        };

        const int num_bidir = static_cast<int>(bidir_refs_.size());

        for (auto& [perm, plan] : wave_plans_) {
            // Build flat active component list (same as solve_perm).
            std::vector<Component*> active(evals_.begin(), evals_.end());
            const int nn = static_cast<int>(active.size());

            // Compute effective masks.
            std::vector<std::array<uint64_t, W>> eff_out(nn), eff_in(nn);
            for (int i = 0; i < nn; ++i)
                for (int w = 0; w < W; ++w) {
                    uint64_t pwr = SignalPool::power_rails[w];
                    eff_out[i][w] = active[i]->outputs()[w] & ~active[i]->inputs()[w] & ~pwr;
                    eff_in[i][w]  = active[i]->inputs()[w] & ~active[i]->async_inputs()[w] & ~pwr;
                }
            for (int b = 0; b < num_bidir; ++b) {
                int ci = -1;
                for (int i = 0; i < nn; ++i)
                    if (active[i] == bidir_refs_[b].comp) { ci = i; break; }
                if (ci < 0) continue;
                BidirDir dir = perm_dir(perm, b);
                for (int w = 0; w < W; ++w) {
                    uint64_t om = bidir_refs_[b].block->out_mask[w];
                    uint64_t im = bidir_refs_[b].block->in_mask[w];
                    switch (dir) {
                        case BidirDir::Output:
                            eff_out[ci][w] |= om;  eff_in[ci][w] &= ~om;
                            eff_out[ci][w] &= ~im; eff_in[ci][w] |= im;
                            break;
                        case BidirDir::Input:
                            eff_out[ci][w] &= ~om; eff_in[ci][w] |= om;
                            eff_out[ci][w] |= im;  eff_in[ci][w] &= ~im;
                            break;
                        case BidirDir::HiZ:
                            eff_out[ci][w] &= ~om; eff_in[ci][w] &= ~om;
                            eff_out[ci][w] &= ~im; eff_in[ci][w] &= ~im;
                            break;
                    }
                }
            }

            // Build depends + in_deg (same as solve_perm).
            std::vector<std::vector<bool>> depends(nn, std::vector<bool>(nn, false));
            std::vector<int> in_deg(nn, 0);
            for (int a = 0; a < nn; ++a)
                for (int b2 = 0; b2 < nn; ++b2) {
                    if (a == b2) continue;
                    for (int w = 0; w < W; ++w)
                        if (eff_out[a][w] & eff_in[b2][w]) {
                            depends[b2][a] = true;
                            break;
                        }
                }
            for (int b2 = 0; b2 < nn; ++b2)
                for (int a = 0; a < nn; ++a)
                    if (depends[b2][a]) ++in_deg[b2];

            // Build component -> active index map for wave lookup.
            std::unordered_map<Component*, int> comp_to_idx;
            for (int i = 0; i < nn; ++i)
                comp_to_idx[active[i]] = i;

            // Reuse dump_cycle_dot with output path in wave_output/.
            std::string dot = "digraph dag {\n  rankdir=LR;\n  newrank=true;\n  node [shape=box fontname=\"Consolas\" fontsize=10];\n  edge [fontname=\"Consolas\" fontsize=8];\n";
            dot += fmt::format("  labelloc=t;\n  label=\"perm {} ({} waves)\";\n", perm, plan.waves.size());
            for (int i = 0; i < nn; ++i) {
                dot += fmt::format("  n{} [label=\"{}\"];\n", i, active[i]->name());
            }
            // Force wave columns with rank=same + subtle wave labels.
            for (size_t w = 0; w < plan.waves.size(); ++w) {
                dot += fmt::format("  subgraph cluster_w{} {{\n", w);
                dot += fmt::format("    label=\"W{}\";\n", w);
                dot += "    style=dashed; color=\"#cccccc\"; fontcolor=\"#999999\"; fontsize=8;\n";
                dot += "    rank=same;\n";
                for (auto* c : plan.waves[w]) {
                    auto it2 = comp_to_idx.find(c);
                    if (it2 != comp_to_idx.end())
                        dot += fmt::format("    n{};\n", it2->second);
                }
                dot += "  }\n";
            }
            for (int i = 0; i < nn; ++i) {
                for (int j = 0; j < nn; ++j) {
                    if (!depends[i][j]) continue;
                    std::string sigs;
                    int sig_count = 0;
                    for (int s = 1; s < SignalPool::count; ++s) {
                        int w = s / 64;
                        uint64_t bit = uint64_t(1) << (s % 64);
                        if ((eff_out[j][w] & bit) && (eff_in[i][w] & bit)) {
                            const char* nm = SignalPool::names[s];
                            if (sig_count < 4) {
                                if (!sigs.empty()) sigs += "\\n";
                                sigs += (nm ? nm : "?");
                            }
                            ++sig_count;
                        }
                    }
                    if (sig_count > 4)
                        sigs += fmt::format("\\n+{} more", sig_count - 4);
                    dot += fmt::format("  n{} -> n{} [label=\"{}\"];\n", j, i, sigs);
                }
            }
            dot += "}\n";

            std::string dot_path = std::string(WAVE_OUTPUT_DIR) + "/perm_" + std::to_string(perm) + ".dot";
            std::string svg_path = std::string(WAVE_OUTPUT_DIR) + "/perm_" + std::to_string(perm) + ".svg";
            {
                std::ofstream f(dot_path);
                f << dot;
            }
            spdlog::info("[Scheduler] wrote {}", dot_path);
            int rc = std::system(fmt::format("\"C:/Program Files/Graphviz/bin/dot.exe\" -Tsvg {} -o {}", dot_path, svg_path).c_str());

            if (rc == 0) {
                spdlog::info("[Scheduler] rendered {}", svg_path);
            }
            std::filesystem::remove(dot_path);
        }
    }

    // Render SVG for every DAG permutation encountered at runtime.
    void dump_permutation_svgs() {
        dump_permutation_dots(evals_, static_cast<int>(evals_.size()));
    }

    // Evaluate all components in topological wave order.
    void evaluate(Fiber caller = nullptr) {
        // Debugger gate: zero-cost when not paused (relaxed load + not-taken branch).
        if (paused_.load(std::memory_order_relaxed)) [[unlikely]]
            pause_gate();

        #if 0
        // Auto-pause on VS debugger resume: if wall time between two cycles
        // exceeds ~50ms worth of TSC ticks, a debugger must have frozen us.
        // __rdtsc() is ~1 cycle, so this is essentially free.
        {
            uint64_t now = __rdtsc();
            if (dbg_last_tsc_ && (now - dbg_last_tsc_) > dbg_tsc_threshold_) [[unlikely]]
                paused_.store(true, std::memory_order_relaxed);
            dbg_last_tsc_ = now;
        }
        #endif

        // Select DAG permutation by checking bidir block lambdas.
        // Map BidirDir bit flags to base-3 digits: HiZ(1)->0, Input(2)->1, Output(4)->2.
        // Lambdas may read pin levels -- suspend validation during selection.
#ifdef BENCH_PIN_VALIDATION
        SignalPool::end_component();
#endif
        // Pre-compute bus address for bidir lambdas (AVX2: 20 bytes in one shot).
        {
            const auto* p = reinterpret_cast<const __m256i*>(&SignalPool::levels[bus_address_base_]);
            SignalPool::bus_address = _mm256_movemask_epi8(
                _mm256_cmpgt_epi8(_mm256_loadu_si256(p), _mm256_setzero_si256())) & 0xFFFFF;
        }
        static constexpr uint64_t dir_to_digit[] = {0, 0, 1, 0, 2};  // indexed by uint8_t(BidirDir)
        uint64_t perm = 0, mul = 1;
        for (int i = 0; i < static_cast<int>(bidir_refs_.size()); ++i) {
            uint8_t d = uint8_t(bidir_refs_[i].block->direction());
            perm += dir_to_digit[d] * mul;
            mul *= 3;
        }

        auto it = wave_plans_.find(perm);
        if (it == wave_plans_.end()) {
            auto plan = solve_perm(perm);
            plan.flatten();
            it = wave_plans_.emplace(perm, std::move(plan)).first;
        }

        // Flattened eval: single contiguous array, plain index loop.
        // No double indirection through vector<vector<Component*>>.
        const auto& flat = it->second.flat;
        const int plan_size = static_cast<int>(flat.size());
        Component* const* plan = flat.data();
        for (int i = 0; i < plan_size; ++i) {
#ifdef BENCH_PIN_VALIDATION
            SignalPool::begin_component(plan[i]);
#endif
            plan[i]->on_cycle(caller);
#ifdef BENCH_PIN_VALIDATION
            SignalPool::end_component();
#endif
        }
    }

    // Dump a dependency cycle as DOT -> SVG for debugging.
    static constexpr int W = Component::SLOT_WORDS;
    static void dump_cycle_dot(Component* const* comps, int n,
                               const std::vector<std::vector<bool>>& depends,
                               const std::vector<int>& in_deg,
                               const std::vector<std::array<uint64_t, W>>& eff_out,
                               const std::vector<std::array<uint64_t, W>>& eff_in) {
        std::string dot = "digraph dag {\n  rankdir=LR;\n  concentrate=true;\n  node [shape=box fontname=\"Consolas\" fontsize=10];\n  edge [fontname=\"Consolas\" fontsize=8];\n";
        for (int i = 0; i < n; ++i) {
            const char* color = (in_deg[i] > 0) ? "red" : "black";
            dot += fmt::format("  n{} [label=\"{}\" color={} fontcolor={}];\n",
                               i, comps[i]->name(), color, color);
        }
        // Collect edges, then assign compass points to spread arrows around boxes.
        struct Edge { int from, to; std::string sigs; const char* color; };
        std::vector<Edge> edges;
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (!depends[i][j]) continue;
                std::string sigs;
                int sig_count = 0;
                for (int s = 1; s < SignalPool::count; ++s) {
                    int w = s / 64;
                    uint64_t bit = uint64_t(1) << (s % 64);
                    if ((eff_out[j][w] & bit) && (eff_in[i][w] & bit)) {
                        const char* nm = SignalPool::names[s];
                        if (sig_count < 4) {
                            if (!sigs.empty()) sigs += "\\n";
                            sigs += (nm ? nm : "?");
                        }
                        ++sig_count;
                    }
                }
                if (sig_count > 4)
                    sigs += fmt::format("\\n+{} more", sig_count - 4);
                const char* ec = (in_deg[i] > 0 && in_deg[j] > 0) ? "red" : "black";
                edges.push_back({j, i, std::move(sigs), ec});
            }
        }
        for (auto& e : edges) {
            dot += fmt::format("  n{} -> n{} [label=\"{}\" color={} fontcolor={}];\n",
                               e.from, e.to, e.sigs, e.color, e.color);
        }
        dot += "}\n";
        std::filesystem::create_directories(WAVE_OUTPUT_DIR);
        std::string dot_path = std::string(WAVE_OUTPUT_DIR) + "/cycle_debug.dot";
        std::string svg_path = std::string(WAVE_OUTPUT_DIR) + "/cycle_debug.svg";
        {
            std::ofstream f(dot_path);
            f << dot;
        }
        spdlog::critical("[Scheduler] Wrote {}", dot_path);
        int rc = std::system(fmt::format("\"C:/Program Files/Graphviz/bin/dot.exe\" -Tsvg {} -o {}", dot_path, svg_path).c_str());
        if (rc == 0) {
            spdlog::critical("[Scheduler] Rendered {}", svg_path);
            std::filesystem::remove(dot_path);
        } else {
            spdlog::critical("[Scheduler] dot failed (rc={}), SVG not generated", rc);
        }
    }

    // --- Debugger pause / single-step ---
    // Zero-cost when not paused: a single relaxed load + predicted-not-taken branch.
    // When paused, the clock thread sleeps via umwait (WAITPKG) monitoring the
    // control word, waking only when the UI writes to it (step/resume).

    bool is_paused() const { return paused_.load(std::memory_order_relaxed); }

    void pause()  { paused_.store(true, std::memory_order_relaxed); }
    void resume() {
        paused_.store(false, std::memory_order_relaxed);
        dbg_last_tsc_ = 0;  // reset so first cycle after resume doesn't re-trigger
        dbg_wake_.store(1, std::memory_order_release);
    }
    // Step one CLK cycle.
    void step_cycle() {
        steps_.fetch_add(1, std::memory_order_relaxed);
        dbg_wake_.store(1, std::memory_order_release);
    }
    // Step one instruction: run cycles until the CPU's instruction counter changes.
    void step_instruction() {
        dbg_instr_start_ = dbg_cpu_->instr_count();
        step_instr_.store(true, std::memory_order_relaxed);
        dbg_wake_.store(1, std::memory_order_release);
    }
    // Step over: run until IP == target_ip with SP >= saved SP.
    // Skips into CALLs/INTs, pauses when they return to the next instruction.
    void step_over(uint16_t target_ip, uint16_t saved_sp) {
        dbg_over_ip_ = target_ip;
        dbg_over_sp_ = saved_sp;
        step_over_.store(true, std::memory_order_relaxed);
        dbg_wake_.store(1, std::memory_order_release);
    }
    void set_cpu(IC_8088* cpu) { dbg_cpu_ = cpu; }

    // Set the pool base index for the 20-bit address bus (LA0-LA19).
    // Called by board wiring so evaluate() can pre-compute bus_address.
    void set_bus_address_base(int base) { bus_address_base_ = base; }

private:
    // Debugger state -- atomic so the render thread can poke them.
    std::atomic<bool> paused_{false};
    std::atomic<int>  steps_{0};
    std::atomic<bool> step_instr_{false};
    std::atomic<bool> step_over_{false};
    std::atomic<int>  dbg_wake_{0};   // written by UI to break umwait
    IC_8088*          dbg_cpu_ = nullptr;
    uint64_t          dbg_instr_start_ = 0;
    uint16_t          dbg_over_ip_ = 0;
    uint16_t          dbg_over_sp_ = 0;
    uint64_t          dbg_last_tsc_ = 0;
    // ~50ms at 3GHz = 150M ticks. Conservative -- any cycle gap this large
    // means a debugger froze us (normal cycle is ~600 ticks at 4.77MHz).
    uint64_t          dbg_tsc_threshold_ = 150'000'000;

    // Block the clock thread while paused, using umwait to sleep efficiently.
    // Called at the top of evaluate(); predicted-not-taken when running normally.
    void pause_gate() {
        // Instruction stepping: let cycles through until instr_count changes.
        if (step_instr_.load(std::memory_order_relaxed)) {
            if (dbg_cpu_ && dbg_cpu_->instr_count() != dbg_instr_start_) {
                step_instr_.store(false, std::memory_order_relaxed);
                // Instruction completed -- fall through to sleep.
            } else {
                return;  // same instruction, keep running
            }
        }

        // Step over: let cycles through until IP == target and SP >= saved.
        // Check only at instruction boundaries (instr_count changed since last check).
        if (step_over_.load(std::memory_order_relaxed)) {
            if (dbg_cpu_ && dbg_cpu_->ip() == dbg_over_ip_
                && dbg_cpu_->regs16_ro()[IC_8088::SP] >= dbg_over_sp_) {
                step_over_.store(false, std::memory_order_relaxed);
                // Target reached -- fall through to sleep.
            } else {
                return;  // keep running
            }
        }

        for (;;) {
            // Consume a pending cycle step if available.
            int s = steps_.load(std::memory_order_relaxed);
            if (s > 0 && steps_.compare_exchange_weak(s, s - 1, std::memory_order_relaxed))
                return;
            if (!paused_.load(std::memory_order_relaxed))
                return;
            // Step requested -- start running.
            if (step_instr_.load(std::memory_order_relaxed))
                return;
            if (step_over_.load(std::memory_order_relaxed))
                return;

            // Sleep until the UI thread writes to dbg_wake_.
            _umonitor(const_cast<int*>(reinterpret_cast<volatile int*>(&dbg_wake_)));
            uint64_t deadline = __rdtsc() + 500000;
            _umwait(1, deadline);
            dbg_wake_.store(0, std::memory_order_relaxed);
        }
    }

    bool unified_resolved_ = false;
    int bus_address_base_ = 0;

    std::vector<CallbackComponent*> callbacks_;

    // Per-permutation wave plans (populated by dump_unified_waves()).
    struct WavePlan {
        std::vector<std::vector<Component*>> waves;

        // Flattened eval plan: single contiguous array of Component*,
        // iterated with a plain index loop.  Eliminates the double
        // indirection through vector<vector<Component*>>.
        std::vector<Component*> flat;

        void flatten() {
            flat.clear();
            for (auto& wave : waves)
                for (auto* c : wave)
                    flat.push_back(c);
        }
    };
    std::unordered_map<uint64_t, WavePlan> wave_plans_;

    // Bidir block references for runtime DAG selection.
    struct BidirRef {
        Component* comp;
        const Component::BidirBlock* block;
    };
    std::vector<BidirRef> bidir_refs_;
    std::vector<Component*> evals_;
    std::vector<int> bidir_comp_idx_;

    WavePlan solve_perm(uint64_t perm) {
        constexpr int W = Component::SLOT_WORDS;
        using BidirDir = Component::BidirDir;
        static constexpr BidirDir digit_to_dir[3] = {
            BidirDir::HiZ, BidirDir::Input, BidirDir::Output
        };
        auto perm_dir = [](uint64_t p, int b) -> BidirDir {
            for (int i = 0; i < b; ++i) p /= 3;
            return digit_to_dir[p % 3];
        };

        // All components participate in every permutation.
        std::vector<Component*> active_evals(evals_.begin(), evals_.end());

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
                uint64_t pwr = SignalPool::power_rails[w];
                eff_out[i][w] = active_evals[i]->outputs()[w] & ~active_evals[i]->inputs()[w] & ~pwr;
                eff_in[i][w]  = active_evals[i]->inputs()[w]  & ~active_evals[i]->async_inputs()[w] & ~pwr;
            }

        for (int b = 0; b < num_bidir; ++b) {
            int ci = bidir_active_idx[b];
            if (ci < 0) continue;  // bidir block's component excluded
            BidirDir dir = perm_dir(perm, b);
            //spdlog::info("[Scheduler] perm {} bidir[{}] comp={} dir={} ci={}",
            //             perm, b, bidir_refs_[b].comp->name(), int(dir), ci);
            for (int w = 0; w < W; ++w) {
                uint64_t om = bidir_refs_[b].block->out_mask[w];
                uint64_t im = bidir_refs_[b].block->in_mask[w];
                //if (om || im)
                //    spdlog::info("[Scheduler]   w={} om=0x{:016X} im=0x{:016X}", w, om, im);
                switch (dir) {
                    case BidirDir::Output:
                        eff_out[ci][w] |= om;  eff_in[ci][w] &= ~om;
                        eff_out[ci][w] &= ~im; eff_in[ci][w] |= im;
                        break;
                    case BidirDir::Input:
                        eff_out[ci][w] &= ~om; eff_in[ci][w] |= om;
                        eff_out[ci][w] |= im;  eff_in[ci][w] &= ~im;
                        break;
                    case BidirDir::HiZ:
                        eff_out[ci][w] &= ~om;  eff_in[ci][w] &= ~om;
                        eff_out[ci][w] &= ~im;  eff_in[ci][w] &= ~im;
                        break;
                }
            }
        }

#if 0
        // Dump effective masks after bidir processing.
        for (int i = 0; i < n; ++i) {
            bool has_any = false;
            for (int w = 0; w < W; ++w)
                if (eff_out[i][w] || eff_in[i][w]) has_any = true;
            if (!has_any) continue;
            for (int w = 0; w < W; ++w) {
                if (eff_out[i][w] || eff_in[i][w])
                    spdlog::info("[Scheduler] perm {} eff[{}] {} w={} out=0x{:016X} in=0x{:016X}",
                                 perm, i, active_evals[i]->name(), w, eff_out[i][w], eff_in[i][w]);
            }
        }
#endif

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
            dump_cycle_dot(active_evals.data(), n, depends, in_deg, eff_out, eff_in);
            // Also dump all previously-successful permutations for comparison.
            dump_permutation_dots(evals_, static_cast<int>(evals_.size()));
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

        #if defined(WAVE_DEBUGS)
        spdlog::debug("[Scheduler] solved perm {} ({} waves):", perm, plan.waves.size());
        for (int w = 0; w < static_cast<int>(plan.waves.size()); ++w) {
            std::string names;
            for (auto* c : plan.waves[w]) {
                if (!names.empty()) names += ", ";
                names += c->name();
            }
            spdlog::debug("[Scheduler]   wave {}: {}", w, names);
        }
        #endif

        return plan;
    }

    std::vector<FiberComponent*> fibers_;
    std::vector<Component*> visuals_;
};

} // namespace bench
