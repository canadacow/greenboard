#pragma once
#include "core/types.h"
#include "core/component.h"
#include <string>
#include <vector>
#include <memory>

namespace bench {

// A physical DIP socket soldered onto the motherboard.
// Has N pins, each wired to a motherboard trace (Signal).
// An IC (Component) can be inserted or ejected.
class Socket {
public:
    // pin_count is the physical DIP pin count (e.g. 40 for 8088).
    // Pins are numbered 1..pin_count (matching DIP convention).
    Socket(std::string ref_designator, std::string ic_label, int pin_count);

    const std::string& ref() const { return ref_; }
    const std::string& label() const { return label_; }
    int pin_count() const { return pin_count_; }

    // Wire pin N (1-based) to a motherboard signal.
    void wire(int pin, Signal& signal);
    Signal* pin_signal(int pin) const;

    // Label a pin for debugging/visualization.
    void pin_name(int pin, const std::string& name);
    const std::string& pin_name(int pin) const;

    // Pin direction (for documentation and future visualization).
    void pin_dir(int pin, PinDir dir);
    PinDir pin_dir(int pin) const;

    // Insert/eject an IC component.
    void insert(std::unique_ptr<Component> ic);
    std::unique_ptr<Component> eject();
    bool occupied() const { return occupant_ != nullptr; }
    Component* occupant() const { return occupant_.get(); }

private:
    std::string ref_;       // e.g. "U3"
    std::string label_;     // e.g. "8088"
    int pin_count_;

    struct PinInfo {
        Signal* signal = nullptr;
        std::string name;
        PinDir dir = PinDir::NoConnect;
    };
    std::vector<PinInfo> pins_; // index 0 = pin 1

    std::unique_ptr<Component> occupant_;
};

} // namespace bench
