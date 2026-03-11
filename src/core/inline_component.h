#pragma once
#include "core/component.h"
#include "core/signal.h"
#include <atomic>

namespace bench {

class Mailbox;

// A combinational logic IC that executes synchronously in the caller's context.
//
// No thread, no mailbox. When an input signal changes, on_signal_change() is
// called inline in the driving thread. This guarantees that all combinational
// propagation completes before any async subscriber (ThreadedComponent) wakes.
//
// Transaction API: callers that drive signals feeding an InlineComponent
// should wrap their drives in begin_transaction() / commit_transaction().
// While a transaction is active, async wakes from any signal driven in
// the same thread are deferred until commit. This batches all combinational
// propagation before any threaded IC sees the changes.
class InlineComponent : public Component {
public:
    explicit InlineComponent(std::string name);
    ~InlineComponent() override = default;

    void power_on() override;
    void power_off() override;
    bool is_powered() const override { return powered_; }

    // Transaction: defer all async wakes until commit.
    void begin_transaction();
    void commit_transaction();

    // Queue an async wake to fire at commit time.
    void defer_wake(Mailbox* mb);

    // Returns the InlineComponent currently in a transaction on this thread,
    // or nullptr if none.
    static InlineComponent* active_transaction();

protected:
    // Routes this component into Signal's sync subscriber list.
    void subscribe_to(Signal& sig) override;

private:
    bool powered_ = false;
    bool in_sync_ = false;  // re-entrancy guard
    std::atomic_flag in_txn_ = ATOMIC_FLAG_INIT;  // atomic latch
    std::vector<Mailbox*> deferred_wakes_;

    static thread_local InlineComponent* tl_active_txn_;

    friend class Signal;
};

} // namespace bench
