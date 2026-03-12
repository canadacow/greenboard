#include "host_platform/fiber.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace bench {

Fiber fiber_convert_thread() {
    return static_cast<Fiber>(::ConvertThreadToFiber(nullptr));
}

void fiber_revert_thread(Fiber /*f*/) {
    ::ConvertFiberToThread();
}

Fiber fiber_create(std::size_t stack_size,
                   void (*entry)(void* user_data),
                   void* user_data) {
    // LPFIBER_START_ROUTINE is void CALLBACK (*)(LPVOID).
    // Our signature matches (cdecl vs CALLBACK differs, but
    // CreateFiber on x64 uses a single calling convention).
    return static_cast<Fiber>(
        ::CreateFiber(stack_size,
                      reinterpret_cast<LPFIBER_START_ROUTINE>(entry),
                      user_data));
}

void fiber_delete(Fiber f) {
    ::DeleteFiber(f);
}

void fiber_switch(Fiber target) {
    ::SwitchToFiber(target);
}

} // namespace bench
