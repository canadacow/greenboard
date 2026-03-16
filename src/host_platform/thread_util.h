#pragma once

// Platform-agnostic thread tuning API.
//
// Thin wrapper over OS primitives (Win32 on Windows).
// No OS headers leak into callers.

namespace bench {

// Raise the calling thread to the highest non-realtime priority.
void thread_set_time_critical();

// Pin the calling thread to performance cores only (P-cores on Intel hybrid).
// No-op if the platform has no hybrid topology or detection fails.
void thread_pin_to_pcores();

} // namespace bench
