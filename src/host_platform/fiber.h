#pragma once
#include <cstddef>

// Platform-agnostic fiber API.
//
// Thin wrapper over OS primitives (Win32 Fibers on Windows).
// All types are opaque pointers -- no OS headers leak into callers.

namespace bench {

// Opaque fiber handle.
using Fiber = void*;

// Convert the calling thread into a fiber. Returns its handle.
// Must be called before any other fiber operations on this thread.
Fiber fiber_convert_thread();

// Revert the calling fiber back to a plain thread.
// The handle from fiber_convert_thread() is invalidated.
void fiber_revert_thread(Fiber f);

// Create a new fiber with the given stack size.
// entry(user_data) is called when the fiber is first switched to.
// Returns nullptr on failure.
Fiber fiber_create(std::size_t stack_size,
                   void (*entry)(void* user_data),
                   void* user_data);

// Delete a fiber created with fiber_create().
// Must not be the currently executing fiber.
void fiber_delete(Fiber f);

// Switch from the current fiber to `target`.
// The current fiber is suspended; it resumes when someone switches back.
void fiber_switch(Fiber target);

} // namespace bench
