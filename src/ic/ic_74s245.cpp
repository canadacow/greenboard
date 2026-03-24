#include "ic/ic_74s245.h"
#include <spdlog/spdlog.h>
#include <cstdlib>

namespace bench {

IC_74S245::IC_74S245()
    : CallbackComponent("74S245") { set_description("Bus Transceiver"); }

void IC_74S245::on_power_on() {
    driving_ = Driving::None;
    pending_driving_ = Driving::None;
}

// Build a PinBlock<8> from socket pins, verifying contiguity.
// Fatal error (not assert) so it fires in release builds.
static PinBlock<8> make_block(Socket& socket, const int (&pins)[8], const char* side) {
    PinBlock<8> pb;
    Signal* s0 = socket.pin_signal(pins[0]);
    if (!s0) {
        spdlog::critical("74S245 {}: pin {} not wired", side, pins[0]);
        std::exit(1);
    }
    pb.base = s0->pin().idx;
    for (int i = 1; i < 8; ++i) {
        Signal* s = socket.pin_signal(pins[i]);
        if (!s) {
            spdlog::critical("74S245 {}: pin {} not wired", side, pins[i]);
            std::exit(1);
        }
        int idx = s->pin().idx;
        if (idx != pb.base + i) {
            spdlog::critical("74S245 {}: pin {} pool index {} != expected {} (not contiguous)",
                             side, pins[i], idx, pb.base + i);
            std::exit(1);
        }
    }
    return pb;
}

void IC_74S245::install(Socket& socket) {
    // Subscribe to all data pins.
    for (int p = 2; p <= 9; ++p) {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
    }
    for (int p = 11; p <= 18; ++p) {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
    }

    // Build PinBlocks in ascending pool-index order.
    // A pins 2-9, B pins 18-11 (pair-matching: A1<->B1, A2<->B2, ...).
    // One side is ascending-by-pin, the other descending -- depends on wiring.
    Signal* a_lo = socket.pin_signal(2);
    Signal* a_hi = socket.pin_signal(9);
    if (!a_lo || !a_hi) {
        spdlog::critical("74S245: A side pins 2/9 not wired");
        std::exit(1);
    }
    bool a_asc = a_lo->pin().idx < a_hi->pin().idx;

    if (a_asc) {
        static constexpr int ap[] = {2,3,4,5,6,7,8,9};
        static constexpr int bp[] = {18,17,16,15,14,13,12,11};
        a_ = make_block(socket, ap, "A");
        b_ = make_block(socket, bp, "B");
    } else {
        static constexpr int ap[] = {9,8,7,6,5,4,3,2};
        static constexpr int bp[] = {11,12,13,14,15,16,17,18};
        a_ = make_block(socket, ap, "A");
        b_ = make_block(socket, bp, "B");
    }

    // Control signals.
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };
    g_   = connect_pin(1);   // ~G
    dir_ = connect_pin(19);  // DIR

    Signal* vcc = socket.pin_signal(20);
    if (vcc) vcc->connect(this);

    declare_async_input(g_); declare_async_input(dir_);
    for (int i = 0; i < 8; ++i) {
        Pin p{a_.base + i};
        declare_input(p); declare_output(p);
    }
    for (int i = 0; i < 8; ++i) {
        Pin p{b_.base + i};
        declare_input(p); declare_output(p);
    }

    // A-side and B-side are anti-correlated:
    //   DIR=High -> A->B (A is input, B is output)
    //   DIR=Low  -> B->A (A is output, B is input)
    // Paired as a single bidir block: is_output=true means A drives (DIR=Low).
    // Bidir direction comes from driving_ (set by bus controller via
    // set_driving()), NOT from the DIR pin.  This ensures the DAG is
    // ordered correctly on the same eval cycle the controller acts.
    Pin a0{a_.base}, a1{a_.base+1}, a2{a_.base+2}, a3{a_.base+3};
    Pin a4{a_.base+4}, a5{a_.base+5}, a6{a_.base+6}, a7{a_.base+7};
    Pin b0{b_.base}, b1{b_.base+1}, b2{b_.base+2}, b3{b_.base+3};
    Pin b4{b_.base+4}, b5{b_.base+5}, b6{b_.base+6}, b7{b_.base+7};
    declare_bidir_pair(
        {a0, a1, a2, a3, a4, a5, a6, a7},   // out_pins (A drives when B->A)
        {b0, b1, b2, b3, b4, b5, b6, b7},   // in_pins  (B drives when A->B)
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

    Level buf[8];
    if (driving_ == Driving::A) {
        b_.read(buf);
        a_.drive(buf);
    }

    if (driving_ == Driving::B) {
        a_.read(buf);
        b_.drive(buf);
    }
}

void IC_74S245::release_outputs() {
    pending_driving_ = Driving::None;
}

void IC_74S245::transfer(bool a_to_b) {
    Level buf[8];
    if (a_to_b) {
        driving_ = Driving::None;
        a_.read(buf);
        bool all_hiz = true;
        for (int i = 0; i < 8; ++i) {
            if (buf[i] != Level::HiZ) { all_hiz = false; break; }
        }
        if (!all_hiz) {
            b_.drive(buf);
            driving_ = Driving::B;
        }
    } else {
        driving_ = Driving::None;
        b_.read(buf);
        bool all_hiz = true;
        for (int i = 0; i < 8; ++i) {
            if (buf[i] != Level::HiZ) { all_hiz = false; break; }
        }
        if (!all_hiz) {
            a_.drive(buf);
            driving_ = Driving::A;
        }
    }
}

} // namespace bench
