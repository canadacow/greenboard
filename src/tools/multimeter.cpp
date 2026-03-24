#include "tools/multimeter.h"
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace bench {

Multimeter::Multimeter(Motherboard& mb) : mb_(mb) {}

Socket* Multimeter::find_socket(const std::string& ref) const {
    // Check all named sockets on the motherboard.
    // This is a linear scan -- fine for a test tool.
    struct Entry { std::string ref; Socket* sock; };
    Socket* sockets[] = {
        &mb_.u1,  &mb_.u2,  &mb_.u3,  &mb_.xu4, &mb_.u5,  &mb_.u6,
        &mb_.u7,  &mb_.u8,  &mb_.u9,  &mb_.u10, &mb_.u11, &mb_.u12,
        &mb_.u13, &mb_.u14, &mb_.u15, &mb_.u16, &mb_.u17, &mb_.u18,
        &mb_.u19, &mb_.u23, &mb_.u24, &mb_.u26, &mb_.u27,
        &mb_.u28, &mb_.u29, &mb_.u30, &mb_.u31, &mb_.u32, &mb_.u33,
        &mb_.u34, &mb_.u35, &mb_.u36,
        &mb_.u46, &mb_.u47, &mb_.u48, &mb_.u49, &mb_.u50, &mb_.u51,
        &mb_.u52, &mb_.u62, &mb_.u63, &mb_.u64, &mb_.u65, &mb_.u66,
        &mb_.u67, &mb_.u79, &mb_.u80, &mb_.u81, &mb_.u82, &mb_.u83,
        &mb_.u84, &mb_.u94, &mb_.u95, &mb_.u96, &mb_.u97, &mb_.u98,
        &mb_.u99, &mb_.u100, &mb_.u101,
        &mb_.td1, &mb_.td2,
    };
    for (Socket* s : sockets) {
        if (s->ref() == ref) return s;
    }
    // RAM banks
    for (auto& s : mb_.ram_bank0) if (s.ref() == ref) return &s;
    for (auto& s : mb_.ram_bank1) if (s.ref() == ref) return &s;
    for (auto& s : mb_.ram_bank2) if (s.ref() == ref) return &s;
    for (auto& s : mb_.ram_bank3) if (s.ref() == ref) return &s;
    return nullptr;
}

Signal* Multimeter::probe(const std::string& ref, int pin) const {
    Socket* s = find_socket(ref);
    if (!s) return nullptr;
    if (pin < 1 || pin > s->pin_count()) return nullptr;
    return s->pin_signal(pin);
}

std::string Multimeter::net_name(const std::string& ref, int pin) const {
    Signal* sig = probe(ref, pin);
    if (!sig) return "";
    return sig->name();
}

bool Multimeter::continuity(const std::string& ref_a, int pin_a,
                            const std::string& ref_b, int pin_b) const {
    Signal* a = probe(ref_a, pin_a);
    Signal* b = probe(ref_b, pin_b);
    if (!a || !b) return false;
    return a == b;
}

void Multimeter::drive(const std::string& ref, int pin, Level lvl) {
    Signal* sig = probe(ref, pin);
    if (sig) sig->drive(lvl);
}

Level Multimeter::read(const std::string& ref, int pin) const {
    Signal* sig = probe(ref, pin);
    if (!sig) return Level::HiZ;
    return sig->level();
}

void Multimeter::release(const std::string& ref, int pin) {
    Signal* sig = probe(ref, pin);
    if (sig) sig->release();
}

// =========================================================================
// Full board continuity audit.
// For every net in the BRD, collect all socket pins on that net,
// then verify they all resolve to the same Signal* object.
// =========================================================================
int Multimeter::audit(const std::string& brd_path) const {
    BrdData brd = parse_brd(brd_path);

    // Build: net_name -> [(ref, pin_num), ...]
    std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> net_pins;
    for (const auto& comp : brd.components) {
        if (comp.ref == "?" || comp.ref.empty()) continue;
        for (const auto& pad : comp.pads) {
            if (pad.net_name.empty() || pad.net_id == 0) continue;
            int pin_num = 0;
            try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
            net_pins[pad.net_name].emplace_back(comp.ref, pin_num);
        }
    }

    int failures = 0;
    int nets_tested = 0;
    int pins_tested = 0;

    for (const auto& [net, pins] : net_pins) {
        // Only test nets that touch sockets (skip passives, connectors, etc.)
        // Collect the Signal* for each socket pin on this net.
        Signal* expected = nullptr;
        bool first = true;

        for (const auto& [ref, pin_num] : pins) {
            Socket* s = find_socket(ref);
            if (!s) continue;  // not a socket (connector, passive, etc.)
            if (pin_num < 1 || pin_num > s->pin_count()) continue;

            Signal* sig = s->pin_signal(pin_num);
            if (!sig) {
                spdlog::warn("OPEN: {}.{} should be on net '{}' but is NC",
                             ref, pin_num, net);
                ++failures;
                continue;
            }

            if (first) {
                expected = sig;
                first = false;
            } else if (sig != expected) {
                spdlog::error("BREAK: {}.{} on net '{}' -- expected Signal '{}' but got '{}'",
                              ref, pin_num, net,
                              expected ? expected->name() : "(null)",
                              sig->name());
                ++failures;
            }
            ++pins_tested;
        }
        if (!first) ++nets_tested;
    }

    spdlog::info("Continuity audit: {} nets, {} pins tested, {} failures",
                 nets_tested, pins_tested, failures);
    return failures;
}

} // namespace bench
