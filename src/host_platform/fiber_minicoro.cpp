#define MINICORO_IMPL
#include "host_platform/minicoro.h"
#include "host_platform/fiber.h"

namespace bench {

// With minicoro, the caller (8284A thread) doesn't need to be a coroutine.
// fiber_convert_thread / fiber_revert_thread become no-ops -- we return a
// dummy non-null handle so callers that null-check it still work.
static char dummy_thread_fiber;

Fiber fiber_convert_thread() {
    return &dummy_thread_fiber;
}

void fiber_revert_thread(Fiber /*f*/) {
    // No-op.
}

// Each fiber wraps an mco_coro*. The user's entry function and user_data
// are stashed so we can adapt the signature (minicoro passes mco_coro*,
// our API passes void*).
struct FiberAdapter {
    void (*entry)(void* user_data);
    void* user_data;
};

static void adapter_entry(mco_coro* co) {
    auto* fa = static_cast<FiberAdapter*>(mco_get_user_data(co));
    fa->entry(fa->user_data);
}

Fiber fiber_create(std::size_t stack_size,
                   void (*entry)(void* user_data),
                   void* user_data) {
    // Allocate adapter on the heap (lives for the coroutine's lifetime).
    auto* fa = new FiberAdapter{entry, user_data};

    mco_desc desc = mco_desc_init(adapter_entry, stack_size);
    desc.user_data = fa;

    mco_coro* co = nullptr;
    mco_result res = mco_create(&co, &desc);
    if (res != MCO_SUCCESS) {
        delete fa;
        return nullptr;
    }
    return static_cast<Fiber>(co);
}

void fiber_delete(Fiber f) {
    auto* co = static_cast<mco_coro*>(f);
    auto* fa = static_cast<FiberAdapter*>(mco_get_user_data(co));
    mco_destroy(co);
    delete fa;
}

void fiber_switch(Fiber target) {
    auto* co = static_cast<mco_coro*>(target);

    // If target is the dummy thread handle, we're yielding back to the caller.
    if (co == reinterpret_cast<mco_coro*>(&dummy_thread_fiber)) {
        mco_yield(mco_running());
        return;
    }

    // Otherwise we're resuming a coroutine.
    mco_resume(co);
}

} // namespace bench
