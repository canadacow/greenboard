#pragma once
#include <cstdint>

namespace bench {

// Tri-state logic level on a wire/trace
enum class Level : uint8_t {
    Low  = 0,
    High = 1,
    HiZ  = 2,   // High-impedance / floating
};

// Physical pin direction from the IC's perspective
enum class PinDir {
    In,
    Out,
    Bidirectional,
    Power,       // VCC/GND -- not signal, just power
    NoConnect,   // NC pin
};

} // namespace bench
