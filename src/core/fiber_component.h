#pragma once
#include "core/component.h"
#include "core/signal.h"
#include "host_platform/fiber.h"

namespace bench {

// A component that runs as a fiber (cooperative multitasking).
//
// Replaces ThreadedComponent for all ICs except the clock generator.
// Fibers run on the 8284A's thread -- no OS threads, no synchronization,
// no deadlocks. Completely deterministic execution order.
//
// Lifecycle:
//   power_on()  -- creates the fiber (does not run it yet)
//   resume()    -- called by Scheduler::evaluate() each CLK edge;
//                  switches to the fiber, which runs until yield()
//   power_off() -- deletes the fiber
//
// Reactive components (default run): loop { yield(); on_cycle(); }
// Active components (e.g. 8088): override run() with their own loop,
// calling yield() wherever they would have called wait_mailbox().
class FiberComponent : public Component {
public:
    explicit FiberComponent(std::string name);
    ~FiberComponent() override;

    void power_on() override;
    void power_off() override;
    bool is_powered() const override { return fiber_ != nullptr; }

    // Called by the wave executor, same as any other component.
    // If a caller fiber is set, resumes the fiber (which runs until yield()).
    // If no caller (pre-clock init), does nothing.
    void on_cycle(Fiber caller) override;

protected:
    // Override for active components (e.g. CPU).
    // Default: loop { yield(); on_cycle(); }
    virtual void run();

    // Yield back to the scheduler (replaces wait_mailbox).
    void yield();

    void subscribe_to(Signal& sig) override;

private:
    static void fiber_entry(void* self);

    Fiber fiber_ = nullptr;
    Fiber return_fiber_ = nullptr;
    bool alive_ = false;
};

} // namespace bench
