#pragma once
// TracedWriter -- mixin for components that own memory the CFG tracer records.
//
// Any component where bytes actually land (DRAM, ISA RAM, video framebuffers)
// inherits this to get the tracer pointer and the position callback. The
// callback supplies what the memory component cannot know on its own: where
// the CPU is. Writes are logged the same way whether the CPU or the 8237A
// drove the bus -- the byte landed either way, and that is what the log is
// for.

#if BENCH_CFG_TRACE

#include "debug/cfg_tracer.h"
#include <functional>

namespace bench {

class TracedWriter {
public:
    struct TraceCtx {
        uint16_t cs;
        uint16_t ip;
        uint64_t instr;
    };

    void set_tracer(CFGTracer* t, std::function<TraceCtx()> ctx) {
        tracer_ = t;
        trace_ctx_ = std::move(ctx);
    }

protected:
    void trace_write(uint32_t phys, uint8_t data) {
        if (!tracer_) return;
        auto ctx = trace_ctx_ ? trace_ctx_() : TraceCtx{0, 0, 0};
        tracer_->on_write(phys, data, ctx.cs, ctx.ip, ctx.instr);
    }

    // Planar cards: the post-pipeline byte for one plane. See
    // CFGTracer::PlaneRec for why the bus byte is not enough.
    void trace_plane_write(uint8_t plane, uint32_t off, uint8_t data) {
        if (!tracer_) return;
        auto ctx = trace_ctx_ ? trace_ctx_() : TraceCtx{0, 0, 0};
        tracer_->on_plane_write(plane, off, data, ctx.cs, ctx.ip, ctx.instr);
    }

    CFGTracer* tracer_ = nullptr;
    std::function<TraceCtx()> trace_ctx_;
};

} // namespace bench

#endif // BENCH_CFG_TRACE
