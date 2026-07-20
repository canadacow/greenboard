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

// Pin the calling thread to efficiency cores only (E-cores on Intel hybrid).
// Power experiment: a maxed E-core stays under the fan-curve knee where a
// boosted P-core does not. No-op without hybrid topology.
void thread_pin_to_ecores();

} // namespace bench
