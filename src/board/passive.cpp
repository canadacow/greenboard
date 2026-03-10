#include "board/passive.h"

namespace bench {

DipSwitch::DipSwitch(std::string ref, int positions)
    : ref_(std::move(ref))
    , switches_(positions)
{
}

void DipSwitch::wire(int position, Signal& signal) {
    switches_.at(position - 1).signal = &signal;
}

void DipSwitch::set(int position, bool on) {
    switches_.at(position - 1).on = on;
}

bool DipSwitch::get(int position) const {
    return switches_.at(position - 1).on;
}

void DipSwitch::apply() {
    for (auto& sw : switches_) {
        if (!sw.signal) continue;
        if (sw.on)
            sw.signal->drive(Level::Low);   // closed: grounded
        else
            sw.signal->release();           // open: floats to pull-up
    }
}

} // namespace bench
