#include "ic/ic_74s245.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S245::IC_74S245()
    : CallbackComponent("74S245") { set_description("Bus Transceiver"); }

void IC_74S245::on_power_on() {
    driving_ = Driving::None;
    pending_driving_ = Driving::None;
}

void IC_74S245::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    // A side: A1=pin2 .. A8=pin9
    for (int i = 0; i < 8; ++i)
        a_[i] = connect_pin(2 + i);

    // B side: B1=pin18, B2=pin17, ..., B8=pin11
    for (int i = 0; i < 8; ++i)
        b_[i] = connect_pin(18 - i);

    g_   = connect_pin(1);   // ~G
    dir_ = connect_pin(19);  // DIR

    Signal* vcc = socket.pin_signal(20);
    if (vcc) vcc->connect(this);

    declare_async_input(g_); declare_async_input(dir_);
    for (int i = 0; i < 8; ++i) { declare_input(a_[i]); declare_output(a_[i]); }
    for (int i = 0; i < 8; ++i) { declare_input(b_[i]); declare_output(b_[i]); }

    // A-side and B-side are anti-correlated:
    //   DIR=High -> A->B (A is input, B is output)
    //   DIR=Low  -> B->A (A is output, B is input)
    // Paired as a single bidir block: is_output=true means A drives (DIR=Low).
    // Bidir direction comes from driving_ (set by bus controller via
    // set_driving()), NOT from the DIR pin.  This ensures the DAG is
    // ordered correctly on the same eval cycle the controller acts.
    declare_bidir_pair(
        {a_[0], a_[1], a_[2], a_[3], a_[4], a_[5], a_[6], a_[7]},   // out_pins (A drives when B->A)
        {b_[0], b_[1], b_[2], b_[3], b_[4], b_[5], b_[6], b_[7]},   // in_pins  (B drives when A->B)
        BidirDir::HiZ | BidirDir::Input | BidirDir::Output,
        [this]() -> BidirDir {
            // Commit pending direction from bus controller.
            driving_ = pending_driving_;
            switch (driving_) {
                case Driving::A: return BidirDir::Output;  // B->A: A is output
                case Driving::B: return BidirDir::Input;   // A->B: B is output
                default:         return BidirDir::HiZ;
            }
        });
}

// Never call release() -- single-driver signal pool means release clobbers
// other components actively driving the same signal.  The bidir DAG ordering
// ensures upstream drivers have already written correct values before we read.
// Direction changes take effect one eval later via the bidir lambda, matching
// real hardware timing (8288 sets DT/~R at T1, ~DEN at T2).

void IC_74S245::set_driving(Driving driving) {
    pending_driving_ = driving;
}

void IC_74S245::on_cycle(Fiber /*caller*/) {
    // Direction is always set externally by the bus controller (8288/8237A)
    // via set_driving().  We never read the DIR pin -- the controller is the
    // authority, and driving_ feeds the bidir lambda for correct DAG ordering.

    if (driving_ == Driving::A) {
        for (int i = 0; i < 8; ++i)
            a_[i].drive(b_[i].level());
    }

    if (driving_ == Driving::B) {
        for (int i = 0; i < 8; ++i)
            b_[i].drive(a_[i].level());
    }
}

void IC_74S245::release_outputs() {
    pending_driving_ = Driving::None;
}

void IC_74S245::transfer(bool a_to_b) {
    if (a_to_b) {
        driving_ = Driving::None;
        bool all_hiz = true;
        for (int i = 0; i < 8; ++i) {
            if (a_[i].level() != Level::HiZ) { all_hiz = false; break; }
        }
        if (!all_hiz) {
            for (int i = 0; i < 8; ++i)
                b_[i].drive(a_[i].level());
            driving_ = Driving::B;
        }
    } else {
        driving_ = Driving::None;
        bool all_hiz = true;
        for (int i = 0; i < 8; ++i) {
            if (b_[i].level() != Level::HiZ) { all_hiz = false; break; }
        }
        if (!all_hiz) {
            for (int i = 0; i < 8; ++i)
                a_[i].drive(b_[i].level());
            driving_ = Driving::A;
        }
    }
}

} // namespace bench
