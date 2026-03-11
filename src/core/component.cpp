#include "core/component.h"

namespace bench {

Component::Component(std::string name)
    : name_(std::move(name))
{}

Component::~Component() = default;

} // namespace bench
