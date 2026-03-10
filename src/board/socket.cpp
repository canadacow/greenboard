#include "board/socket.h"
#include "core/signal.h"
#include "core/component.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace bench {

Socket::Socket(std::string ref_designator, std::string ic_label, int pin_count)
    : ref_(std::move(ref_designator))
    , label_(std::move(ic_label))
    , pin_count_(pin_count)
    , pins_(pin_count)
{
}

void Socket::wire(int pin, Signal& signal) {
    pins_.at(pin - 1).signal = &signal;
}

Signal* Socket::pin_signal(int pin) const {
    return pins_.at(pin - 1).signal;
}

void Socket::pin_name(int pin, const std::string& name) {
    pins_.at(pin - 1).name = name;
}

const std::string& Socket::pin_name(int pin) const {
    return pins_.at(pin - 1).name;
}

void Socket::pin_dir(int pin, PinDir dir) {
    pins_.at(pin - 1).dir = dir;
}

PinDir Socket::pin_dir(int pin) const {
    return pins_.at(pin - 1).dir;
}

void Socket::insert(std::unique_ptr<Component> ic) {
    if (occupant_) {
        spdlog::warn("[{}] ejecting {} to insert new IC", ref_, occupant_->name());
        eject();
    }
    spdlog::info("[{}] inserted {}", ref_, ic->name());
    occupant_ = std::move(ic);
}

std::unique_ptr<Component> Socket::eject() {
    if (!occupant_) return nullptr;
    spdlog::info("[{}] ejected {}", ref_, occupant_->name());
    return std::move(occupant_);
}

} // namespace bench
