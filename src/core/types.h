#pragma once
#include <cstdint>

namespace bench {

// Tri-state logic level on a wire/trace
enum class Level : int8_t {
    Low  = -1,
    High = 1,
    HiZ  = 0,   // High-impedance / floating
};

inline constexpr int8_t operator+(Level a, Level b) { return int8_t(a) + int8_t(b); }
inline constexpr int8_t operator+(int8_t a, Level b) { return a + int8_t(b); }
inline constexpr bool operator<(Level a, Level b) { return int8_t(a) < int8_t(b); }

// Physical pin direction from the IC's perspective
enum class PinDir {
    In,
    Out,
    Bidirectional,
    Power,       // VCC/GND -- not signal, just power
    NoConnect,   // NC pin
};

} // namespace bench
