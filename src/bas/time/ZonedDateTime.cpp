#include "ZonedDateTime.hpp"

#include "HHDates.hpp"

#include <memory>

namespace bas::time {

int ZonedDateTime::compareTo(const ZonedDateTime& other) const {
    return toOffsetDateTime()->compareTo(*other.toOffsetDateTime());
}

std::string ZonedDateTime::toIsoString() const {
    auto zoneId = this->zoneId();
    if (zoneId.empty()) {
        zoneId = "Z";
    }

    return toOffsetDateTime()->toIsoString() + "[" + zoneId + "]";
}

bool ZonedDateTime::isValidIsoString(const std::string& text) {
    return HHZonedDateTime::isValidIsoString(text);
}

std::unique_ptr<ZonedDateTime> ZonedDateTime::parseIsoString(const std::string& text) {
    return std::make_unique<HHZonedDateTime>(HHZonedDateTime::parseIsoString(text));
}

std::unique_ptr<ZonedDateTime>
ZonedDateTime::fromTimePoint(const std::chrono::system_clock::time_point& timePoint) {
    auto zt = date::make_zoned(date::current_zone(), timePoint);
    return std::make_unique<HHZonedDateTime>(HHZonedDateTime(zt));
}

} // namespace bas::time
