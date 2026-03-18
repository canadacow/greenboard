#include "ic/ic_74s245.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_74S245::IC_74S245()
    : CallbackComponent("74S245") { set_description("Bus Transceiver"); }

void IC_74S245::on_power_on() {
    driving_ = Driving::None;
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
    declare_bidir_pair(
        {a_[0], a_[1], a_[2], a_[3], a_[4], a_[5], a_[6], a_[7]},   // out_pins (A drives when DIR=Low)
        {b_[0], b_[1], b_[2], b_[3], b_[4], b_[5], b_[6], b_[7]},   // in_pins  (B drives when DIR=High)
        BidirDir::HiZ | BidirDir::Input | BidirDir::Output,
        [this]() -> BidirDir {
            auto gv = g_.level();
            auto dv = dir_.level();
            BidirDir result = BidirDir::HiZ;
            if (gv == Level::Low)
                result = (dv == Level::Low) ? BidirDir::Output : BidirDir::Input;
            spdlog::trace("[{}] bidir lambda: ~G={} DIR={} -> {} g.idx={} dir.idx={}",
                          name(), int(gv), int(dv),
                          result == BidirDir::HiZ ? "HiZ" : (result == BidirDir::Output ? "OUT(A->B)" : "IN(B->A)"),
                          g_.idx, dir_.idx);
            return result;
        });
}

// Never call release() -- single-driver signal pool means release clobbers
// other components actively driving the same signal.  The bidir DAG ordering
// ensures upstream drivers have already written correct values before we read.
// Direction changes take effect one eval later via the bidir lambda, matching
// real hardware timing (8288 sets DT/~R at T1, ~DEN at T2).

void IC_74S245::on_signal_change(Fiber /*caller*/) {
    if (g_.level() != Level::Low) {
        spdlog::trace("[{}] ~G={} -> disabled", name(), int(g_.level()));
        driving_ = Driving::None;
        return;
    }
    update_outputs();
}

void IC_74S245::update_outputs() {

    uint8_t val = 0;

    if (driving_ == Driving::A) {
        for (int i = 0; i < 8; ++i) {
            Level t = b_[i].level();
            if (t != Level::HiZ) {
                a_[i].drive(b_[i].level());    
            }
        }
        spdlog::trace("[{}] B->A: 0x{:02X} (~G={} DIR={}) a[0].idx={} b[0].idx={}", name(), val, int(g_.level()), int(dir_.level()), a_[0].idx, b_[0].idx);
    }

    if (driving_ == Driving::B) {
        for (int i = 0; i < 8; ++i) {
            Level t = a_[i].level();
            if (t != Level::HiZ) {
                b_[i].drive(a_[i].level());    
            }
        }
        spdlog::trace("[{}] A->B: 0x{:02X} (~G={} DIR={}) a[0].idx={} b[0].idx={}", name(), val, int(g_.level()), int(dir_.level()), a_[0].idx, b_[0].idx);        
    }

    switch(dir_.level())
    {
        case Level::High:
            driving_ = Driving::B;
            break;
        case Level::Low:
            driving_ = Driving::A;
            break;
        default:
            driving_ = Driving::None;
            break;
    }
}

void IC_74S245::release_outputs() {
    driving_ = Driving::None;
}

void IC_74S245::transfer(bool a_to_b) {
    if (a_to_b) {
        driving_ = Driving::None;
        uint8_t val = 0;
        bool all_hiz = true;
        for (int i = 0; i < 8; ++i) {
            Level lv = a_[i].level();
            if (lv != Level::HiZ) all_hiz = false;
            if (lv == Level::High) val |= (1 << i);
        }
        if (all_hiz) {
            spdlog::trace("[{}] transfer A->B: HiZ (source floating)", name());
        } else {
            for (int i = 0; i < 8; ++i)
                b_[i].drive(a_[i].level());
            spdlog::trace("[{}] transfer A->B: 0x{:02X}", name(), val);
            driving_ = Driving::B;
        }
    } else {
        driving_ = Driving::None;
        uint8_t val = 0;
        bool all_hiz = true;
        for (int i = 0; i < 8; ++i) {
            Level lv = b_[i].level();
            if (lv != Level::HiZ) all_hiz = false;
            if (lv == Level::High) val |= (1 << i);
        }
        if (all_hiz) {
            spdlog::trace("[{}] transfer B->A: HiZ (source floating)", name());
        } else {
            for (int i = 0; i < 8; ++i)
                a_[i].drive(b_[i].level());
            spdlog::trace("[{}] transfer B->A: 0x{:02X}", name(), val);
            driving_ = Driving::A;
        }
    }
}

} // namespace bench
