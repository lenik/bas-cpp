#include "ZonedTime.hpp"

#include "HHDates.hpp"

#include <memory>

namespace bas::time {

int ZonedTime::compareTo(const ZonedTime& other) const {
    return toOffsetTime()->compareTo(*other.toOffsetTime());
}

std::string ZonedTime::toIsoString() const {
    auto zoneId = this->zoneId();
    if (zoneId.empty()) {
        zoneId = "Z";
    }
    return toOffsetTime()->toIsoString() + "[" + zoneId + "]";
}

bool ZonedTime::isValidIsoString(const std::string& text) {
    return HHZonedTime::isValidIsoString(text);
}

std::unique_ptr<ZonedTime> ZonedTime::parseIsoString(const std::string& text) {
    return std::make_unique<HHZonedTime>(HHZonedTime::parseIsoString(text));
}

} // namespace bas::time
