#include "ic/ic_dip_switch.h"

namespace bench {

IC_DipSwitch::IC_DipSwitch(const char* name, int positions)
    : CallbackComponent(name)
    , count_(positions > 8 ? 8 : positions)
{
    set_description("DIP Switch");
}

void IC_DipSwitch::connect_position(int pos, Signal& signal) {
    signal.connect(this);
    pos_[pos].pin = signal.pin();
    declare_output(pos_[pos].pin);
}

void IC_DipSwitch::set(int pos, bool on) {
    pos_[pos].on = on;
}

bool IC_DipSwitch::get(int pos) const {
    return pos_[pos].on;
}

void IC_DipSwitch::set_value(uint8_t val) {
    for (int i = 0; i < count_; ++i)
        pos_[i].on = !((val >> i) & 1);   // bit=1 means OFF (High), ON=Low
}

uint8_t IC_DipSwitch::value() const {
    uint8_t v = 0;
    for (int i = 0; i < count_; ++i)
        if (!pos_[i].on) v |= (1 << i);   // OFF = High = bit set
    return v;
}

void IC_DipSwitch::on_power_on() {
    drive_all();
}

void IC_DipSwitch::on_power_off() {
    for (int i = 0; i < count_; ++i)
        pos_[i].pin.drive(Level::HiZ);
}

void IC_DipSwitch::on_cycle(Fiber /*caller*/) {
    //drive_all();
}

void IC_DipSwitch::drive_all() {
    for (int i = 0; i < count_; ++i)
        pos_[i].pin.drive(pos_[i].on ? Level::Low : Level::High);
}

} // namespace bench
