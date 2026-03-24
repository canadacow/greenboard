#pragma once
#include "core/component.h"
#include "core/signal.h"
#include "core/mailbox.h"
#include <thread>
#include <atomic>
#include <semaphore>

namespace bench {

// A sequential/clocked IC that runs on its own thread.
//
// Reactive components (default): block on the mailbox, wake on signal
// changes, call on_cycle() to check pin levels.
// Active components (e.g. oscillators): override run() with their own loop.
class ThreadedComponent : public Component {
public:
    explicit ThreadedComponent(std::string name);
    ~ThreadedComponent() override { power_off(); }

    void power_on() override;
    void power_off() override;
    bool is_powered() const override { return thread_.joinable(); }

    Mailbox* mailbox() const { return mailbox_; }

protected:
    // Override for active components (oscillators, etc.) that need their own loop.
    // Default implementation blocks on the mailbox waiting for signal events.
    virtual void run(std::stop_token stop);

    // Block until a connected signal changes, then return.
    void wait_mailbox(std::stop_token& stop);

    // Ack the deferred pending from the last wait_mailbox wake.
    void flush_pending_ack();

    // Check if stop has been requested on this component's thread.
    bool stop_requested() const;

    // Routes this component into Signal's async subscriber list.
    void subscribe_to(Signal& sig) override;

private:
    std::jthread thread_;
    Mailbox* mailbox_;
    bool pending_ack_ = false;
};

} // namespace bench
