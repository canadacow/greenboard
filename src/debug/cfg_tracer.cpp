#include "debug/cfg_tracer.h"
#include <spdlog/spdlog.h>

namespace bench {

CFGTracer::CFGTracer()
    : gen_(ADDR_SPACE, 0), executed_(ADDR_SPACE, 0) {
    nodes_.reserve(1u << 16);
    edges_.reserve(1u << 16);
    ports_.reserve(1u << 22);
    // Reserve generously up front so a long run never reallocates mid-trace.
    // Sized for ~256M instructions: 1 GB of timeline, 10 GB of register state,
    // 4 GB of writes. Trivial against 255 GB of RAM.
    exec_.reserve(1ull << 28);
    states_.reserve(1ull << 28);
    writes_.reserve(1ull << 27);
}

void CFGTracer::exclude_range(uint32_t lo, uint32_t hi_exclusive) {
    excludes_.push_back({lo, hi_exclusive});
}

void CFGTracer::on_instruction(uint32_t cs_ip, uint16_t cs, uint16_t ip,
                               uint64_t instr, bool in_handler) {
    // Execution timeline. Recorded for EVERY instruction -- excluded regions
    // and interrupt handlers included -- so the index stays equal to the
    // CPU's instruction count and a scrub to instant N lands on exactly what
    // was executing.
    if (exec_.size() == instr)
        exec_.push_back(cs_ip);

    // Inside a handler: no node, no edge, and leave prev_visible_ alone so
    // the interrupted code's flow resumes across the excursion untouched.
    if (in_handler) {
        ++handler_instrs_;
        return;
    }

    if (excluded(cs_ip)) {
        // Inside an excluded region (BIOS). Emit nothing, but leave
        // prev_visible_ alone so the return stitches into a single edge.
        in_excluded_ = true;
        return;
    }

    // Edge: record every observed predecessor->successor pair. Whether a
    // given edge is a fall-through or a branch is a question the offline
    // disassembler answers exactly (target == source + instruction length);
    // guessing it here from a predicted address gets interrupts and
    // multi-prefix instructions wrong. Edges dedup into a set, so the cost is
    // bounded by unique control flow, not by execution count.
    //
    // An excursion into an excluded region (BIOS) collapses to a single edge,
    // because nothing inside it updated prev_visible_.
    if (prev_visible_ != ~0u && prev_visible_ != cs_ip) {
        edges_.insert((uint64_t(prev_visible_) << 32) | cs_ip);
    }
    in_excluded_ = false;

    // Node: keyed on (address, generation). A byte that was overwritten since
    // it last executed has a bumped generation, so overlay B never aliases
    // onto overlay A's node.
    const uint32_t g = gen_[cs_ip];
    const uint64_t key = node_key(cs_ip, g);
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        nodes_.emplace(key, Node{cs_ip, cs, ip, g, instr, 1});
    } else {
        ++it->second.hits;
    }

    executed_[cs_ip] = 1;
    prev_visible_ = cs_ip;
}

void CFGTracer::on_write(uint32_t addr, uint8_t data,
                         uint16_t cs, uint16_t ip, uint64_t instr) {
    addr &= 0xFFFFF;

    // Self-modifying code / overlay load: this byte previously executed and is
    // now being overwritten. Retire the CFG node that covered it.
    if (executed_[addr]) {
        ++gen_[addr];
        executed_[addr] = 0;
        ++dirty_events_;
    }

    writes_.push_back(WriteRec{instr, addr, cs, ip, data});
}

void CFGTracer::on_state(uint32_t addr, const uint16_t* r, uint16_t flags,
                         uint64_t instr) {
    // Register file order matches IC_8088::Reg16:
    //   AX CX DX BX SP BP SI DI ES CS SS DS
    states_.push_back(StateRec{
        instr, addr,
        r[0], r[1], r[2], r[3],
        r[4], r[5], r[6], r[7],
        r[8], r[9], r[10], r[11],
        flags, 0,
    });
}

void CFGTracer::on_port_read(uint16_t port, uint8_t data,
                             uint16_t cs, uint16_t ip, uint64_t instr) {
    ports_.push_back(PortRec{instr, port, cs, ip, data, 0});
}

void CFGTracer::on_port_write(uint16_t port, uint8_t data,
                              uint16_t cs, uint16_t ip, uint64_t instr) {
    ports_.push_back(PortRec{instr, port, cs, ip, data, 1});
}

// ------------------------------------------------------------------------
// Output
//
// Binary, little-endian, section-tagged. Layout:
//
//   magic   "BCFG"        4 bytes
//   version u32           1
//   4 sections, each: tag(4) count(u64) then count fixed-size records
//     "NODE"  cs_ip u32, cs u16, ip u16, gen u32, first_seen u64, hits u64
//     "EDGE"  from u32, to u32
//     "WRIT"  instr u64, addr u32, cs u16, ip u16, data u8, src u8, pad u16
//     "PORT"  instr u64, port u16, cs u16, ip u16, data u8, pad u8
// ------------------------------------------------------------------------

namespace {

template <typename T>
inline void put(std::FILE* f, const T& v) {
    std::fwrite(&v, sizeof(T), 1, f);
}

inline void put_tag(std::FILE* f, const char (&tag)[5]) {
    std::fwrite(tag, 1, 4, f);
}

} // namespace

bool CFGTracer::dump(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        spdlog::error("[CFG] cannot open {} for writing", path);
        return false;
    }

    std::fwrite("BCFG", 1, 4, f);
    put<uint32_t>(f, 1);

    put_tag(f, "NODE");
    put<uint64_t>(f, nodes_.size());
    for (const auto& [key, n] : nodes_) {
        put(f, n.cs_ip);
        put(f, n.cs);
        put(f, n.ip);
        put(f, n.gen);
        put(f, n.first_seen);
        put(f, n.hits);
    }

    put_tag(f, "EDGE");
    put<uint64_t>(f, edges_.size());
    for (uint64_t e : edges_) {
        put<uint32_t>(f, uint32_t(e >> 32));
        put<uint32_t>(f, uint32_t(e & 0xFFFFFFFFu));
    }

    put_tag(f, "WRIT");
    put<uint64_t>(f, writes_.size());
    for (const auto& w : writes_) {
        put(f, w.instr);
        put(f, w.addr);
        put(f, w.cs);
        put(f, w.ip);
        put(f, w.data);
        put<uint8_t>(f, 0);
        put<uint16_t>(f, 0);
    }

    // Execution timeline. Bulk-written: index k is instruction k, so there is
    // nothing per-record to encode.
    put_tag(f, "EXEC");
    put<uint64_t>(f, exec_.size());
    if (!exec_.empty())
        std::fwrite(exec_.data(), sizeof(uint32_t), exec_.size(), f);

    // Register state per traced instruction. Bulk-written, which requires the
    // struct to have no implicit padding -- asserted so a layout change here
    // cannot silently desynchronise the reader.
    // instr(8) + addr(4) + 12 registers + flags + _pad (14 x u16) = 40.
    static_assert(sizeof(StateRec) == 8 + 4 + 14 * 2,
                  "StateRec has implicit padding; the reader expects 40 bytes "
                  "field-for-field");
    put_tag(f, "STAT");
    put<uint64_t>(f, states_.size());
    if (!states_.empty())
        std::fwrite(states_.data(), sizeof(StateRec), states_.size(), f);

    put_tag(f, "PORT");
    put<uint64_t>(f, ports_.size());
    for (const auto& p : ports_) {
        put(f, p.instr);
        put(f, p.port);
        put(f, p.cs);
        put(f, p.ip);
        put(f, p.data);
        put(f, p.is_write);
        put<uint16_t>(f, 0);
    }

    std::fclose(f);

    spdlog::info("[CFG] wrote {}: {} nodes, {} edges, {} instrs "
                 "({} in handlers, excluded from CFG), {} writes, "
                 "{} port ops, {} reg states, {} code-overwrite events",
                 path, nodes_.size(), edges_.size(), exec_.size(),
                 handler_instrs_, writes_.size(), ports_.size(),
                 states_.size(), dirty_events_);
    return true;
}

} // namespace bench
