#include "core/signal.h"
#include "core/component.h"
#include "core/scheduler.h"
#include <cassert>
#include <spdlog/spdlog.h>

namespace bench {

// --- SignalPool ---

alignas(64) Level SignalPool::levels[MAX_SIGNALS] = {};
const char* SignalPool::names[MAX_SIGNALS] = {};
int SignalPool::count = 1;  // slot 0 reserved as dummy (reads HiZ, writes vanish)
uint64_t SignalPool::power_rails[PW] = {};

// Static init: slot 0 must read HiZ (default zero-init gives Level::Low = 0).
static struct Slot0Init { Slot0Init() { SignalPool::levels[0] = Level::HiZ; } } slot0_init_;

#ifdef BENCH_PIN_VALIDATION
thread_local bool SignalPool::is_clock_thread_ = false;
bool SignalPool::validation_enabled_ = true;
Component* SignalPool::active_comp_ = nullptr;
uint64_t SignalPool::valid_write_[SLOT_WORDS] = {};
uint64_t SignalPool::valid_read_[SLOT_WORDS] = {};
uint64_t SignalPool::valid_hiz_release_[SLOT_WORDS] = {};

static bool is_power_rail(int idx) {
    const char* n = SignalPool::names[idx];
    if (!n) return false;
    return (n[0] == '+' || (n[0] == 'G' && n[1] == 'N' && n[2] == 'D'));
}

// Pins that are always driven on real hardware but use bidir HiZ
// to break DAG cycles. Writes are allowed even when bidir says HiZ.
static bool is_write_exempt(int idx) {
    // Pins with bidir HiZ for DAG cycle breaking but legitimately driven
    // in all states. The bidir doesn't mean "don't drive" -- it means
    // "remove DAG edges so the topological sort doesn't cycle."
    Component* c = SignalPool::active_comp_;
    if (!c) return false;
    int word = idx / 64;
    uint64_t bit = uint64_t(1) << (idx % 64);
    // If the component has a base declare_output for this pin AND the
    // bidir overrode it to HiZ, the write is exempt.
    return (c->outputs()[word] & bit) != 0;
}

void SignalPool::print_out_pin_and_exit(bool wrongThread, int word, int idx, uint64_t bit, const Level* lvl)
{
    auto comp_name = active_comp_ ? active_comp_->name() : "Unknown";
    const char* pin_name = names[idx] ? names[idx] : "???";

    if (wrongThread)
    {
        if (lvl)
        {
            spdlog::critical("[PinValidation] Pin writes to {} ({}) with lvl={} from non-simulation thread.", idx, pin_name, (int)*lvl);
        }
        else
        {
            spdlog::critical("[PinValidation] Pin reads to {} ({}) from non-simulation thread.", idx, pin_name);
        }
    }
    else if (lvl)
    {
        uint64_t comp_out = active_comp_ ? active_comp_->outputs()[word] : UINT64_MAX;
        spdlog::critical("[PinValidation] {} writing slot {} ({}) without output declaration",
            comp_name, idx, pin_name);
        spdlog::critical("[PinValidation]   outputs[{}] = 0x{:016X}, valid_write[{}] = 0x{:016X}, bit = 0x{:016X}",
            word, comp_out, word, valid_write_[word], bit);
        spdlog::critical("[PinValidation]   valid_hiz_release[{}] = 0x{:016X}, lvl={}",
            word, valid_hiz_release_[word], (int)*lvl);
    }
    else
    {
        uint64_t comp_in = active_comp_ ? active_comp_->inputs()[word] : UINT64_MAX;
        spdlog::critical("[PinValidation] {} reading slot {} ({}) without input declaration",
            comp_name, idx, pin_name);
        spdlog::critical("[PinValidation]   inputs[{}] = 0x{:016X}, bit = 0x{:016X}",
            word, comp_in, bit);
    }
    std::_Exit(1);
}

void SignalPool::check_write(int idx, Level lvl) {
    if (!validation_enabled_) return;

    uint64_t bit = uint64_t(1) << (idx % 64);
    int word = idx / 64;

    if (!is_clock_thread_) {
        print_out_pin_and_exit(true, word, idx, bit, &lvl);
    }

    if (!active_comp_ || idx == 0 || is_power_rail(idx)) return;

    if (valid_write_[word] & bit) return;
    if (lvl == Level::HiZ && (valid_hiz_release_[word] & bit)) return;
    if (is_write_exempt(idx)) return;

    print_out_pin_and_exit(false, word, idx, bit, &lvl);
}

void SignalPool::check_read(int idx) {
    if (!validation_enabled_) return;

    uint64_t bit = uint64_t(1) << (idx % 64);
    int word = idx / 64;

    if (!is_clock_thread_) {
        print_out_pin_and_exit(true, word, idx, bit, nullptr);
    }

    if (!active_comp_ || idx == 0 || is_power_rail(idx)) return;

    if (valid_read_[word] & bit) return;
    print_out_pin_and_exit(false, word, idx, bit, nullptr);
}

void SignalPool::begin_component(Component* c) {
    active_comp_ = c;
    constexpr int W = SLOT_WORDS;
    // Base: outputs are writable, inputs are readable.
    for (int w = 0; w < W; ++w) {
        valid_write_[w] = c->outputs()[w];
        valid_read_[w]  = c->inputs()[w];
        valid_hiz_release_[w] = 0;
    }
    // Apply bidir block overrides based on current direction.
    for (auto& block : c->bidir_blocks()) {
        auto dir = block.direction();
        for (int w = 0; w < W; ++w) {
            uint64_t om = block.out_mask[w];
            uint64_t im = block.in_mask[w];
            switch (dir) {
                case Component::BidirDir::Output:
                    valid_write_[w] |= om;
                    valid_read_[w]  &= ~om;
                    valid_write_[w] &= ~im;
                    valid_read_[w]  |= im;
                    break;
                case Component::BidirDir::Input:
                    valid_write_[w] &= ~om;
                    valid_read_[w]  |= om;
                    valid_write_[w] |= im;
                    valid_read_[w]  &= ~im;
                    break;
                case Component::BidirDir::HiZ:
                    valid_write_[w] &= ~om;
                    valid_read_[w]  &= ~om;
                    valid_write_[w] &= ~im;
                    valid_read_[w]  &= ~im;
                    // Track HiZ pins -- release (drive HiZ) is allowed.
                    valid_hiz_release_[w] |= om | im;
                    break;
            }
        }
    }
}
#endif

// --- Signal ---

std::atomic<int> Signal::pending_count{0};
Scheduler* Signal::scheduler_ = nullptr;

Signal::Signal(std::string name) : name_(std::move(name)) {
    int idx = SignalPool::allocate();
    assert(idx < SignalPool::MAX_SIGNALS && "Signal pool exhausted");
    level_ = &SignalPool::levels[idx];
    *level_ = Level::HiZ;
    SignalPool::names[idx] = name_.c_str();
}

Signal::Signal(std::string name, int slot) : name_(std::move(name)) {
    assert(slot > 0 && slot < SignalPool::MAX_SIGNALS && "Invalid pre-allocated slot");
    level_ = &SignalPool::levels[slot];
    *level_ = Level::HiZ;
    SignalPool::names[slot] = name_.c_str();
}

void Signal::set_pull(Level pull) {
    pull_ = pull;
    if (*level_ == Level::HiZ && pull != Level::HiZ) {
        drive(pull);
    }
}

void Signal::reset() {
    *level_ = Level::HiZ;
}

void Signal::connect(Component* c) {
    c->subscribe_to(*this);
}

void Signal::disconnect(Component* c) {
    // Only async subscribers need disconnect for now.
}

// --- Bus ---

Bus::Bus(const std::string& prefix, int width) {
    lines_.reserve(width);
    for (int i = 0; i < width; ++i) {
        lines_.push_back(std::make_unique<Signal>(prefix + std::to_string(i)));
    }
}

Bus::Bus(const std::string& prefix, int width, int base_slot) {
    lines_.reserve(width);
    for (int i = 0; i < width; ++i) {
        lines_.push_back(std::make_unique<Signal>(prefix + std::to_string(i), base_slot + i));
    }
}

void Bus::drive(uint32_t value) {
    for (int i = 0; i < width(); ++i) {
        lines_[i]->drive((value >> i) & 1 ? Level::High : Level::Low);
    }
}

void Bus::release() {
    for (auto& line : lines_)
        line->release();
}

void Bus::reset() {
    for (auto& line : lines_)
        line->reset();
}

uint32_t Bus::read() const {
    uint32_t val = 0;
    for (int i = 0; i < width(); ++i) {
        if (lines_[i]->level() == Level::High)
            val |= (1u << i);
    }
    return val;
}

void Bus::connect(Component* c) {
    for (auto& line : lines_)
        line->connect(c);
}

void Bus::disconnect(Component* c) {
    for (auto& line : lines_)
        line->disconnect(c);
}

} // namespace bench
