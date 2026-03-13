#pragma once
#include "core/callback_component.h"
#include "core/signal.h"

namespace bench {

// A bus controller IC (8288) that runs BEFORE inlines settle.
//
// In real hardware, the bus controller's outputs (ALE, ~DEN, ~IOW, etc.)
// propagate through combinational decode logic (74S138 -> ~CS) within
// the same clock period. Bus peripherals (8259A, 8253, etc.) see the
// fully settled bus -- command strobes AND decoded chip selects together.
//
// To match this, the scheduler runs bus controllers first, then settles
// inlines (address decode), then runs regular callbacks (bus peripherals).
//
// Bus controller outputs are committed immediately (written to both
// pending[] and current[]) so inlines see them without a full pool commit.
class BusControllerComponent : public CallbackComponent {
public:
    explicit BusControllerComponent(std::string name)
        : CallbackComponent(std::move(name)) {}
};

} // namespace bench
