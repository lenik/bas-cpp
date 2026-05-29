#include "OffsetTime.hpp"

#include "HHDates.hpp"

#include <memory>

namespace bas::time {

std::unique_ptr<Temporal> OffsetTime::withOffsetSameLocal(int32_t newOffsetSeconds) const {
    return with(TemporalField::OffsetSeconds, newOffsetSeconds);
}

std::unique_ptr<Temporal> OffsetTime::withOffsetSameInstant(int32_t newOffsetSeconds) const {
    const auto delta = static_cast<int64_t>(newOffsetSeconds) - offsetSeconds();
    auto shifted = plus(-delta, TemporalUnit::Seconds);
    return shifted->with(TemporalField::OffsetSeconds, newOffsetSeconds);
}

int OffsetTime::compareTo(const OffsetTime& other) const {
    if (toLocalTime()->toNanoOfDay() != other.toLocalTime()->toNanoOfDay()) {
        return toLocalTime()->toNanoOfDay() < other.toLocalTime()->toNanoOfDay() ? -1 : 1;
    }
    if (offsetSeconds() != other.offsetSeconds()) {
        return offsetSeconds() < other.offsetSeconds() ? -1 : 1;
    }
    return 0;
}

std::string OffsetTime::toIsoString() const {
    auto sec = offsetSeconds();
    auto min = sec / 60;
    auto hour = min / 60;
    min = min % 60;
    sec = sec % 60;

    auto hourStr = std::to_string(hour);
    if (hour < 10) {
        hourStr = "0" + hourStr;
    }
    auto minStr = std::to_string(min);
    if (min < 10) {
        minStr = "0" + minStr;
    }
    return toLocalTime()->toIsoString() + "+" + hourStr + ":" + minStr;
}

bool OffsetTime::isValidIsoString(const std::string& text) {
    return HHOffsetTime::isValidIsoString(text);
}

std::unique_ptr<OffsetTime> OffsetTime::parseIsoString(const std::string& text) {
    return std::make_unique<HHOffsetTime>(HHOffsetTime::parseIsoString(text));
}

} // namespace bas::time
