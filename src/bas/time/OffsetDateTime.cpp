#include "OffsetDateTime.hpp"

#include "HHDates.hpp"

#include <memory>

namespace bas::time {

std::unique_ptr<Temporal> OffsetDateTime::withOffsetSameLocal(int32_t newOffsetSeconds) const {
    return with(TemporalField::OffsetSeconds, newOffsetSeconds);
}

std::unique_ptr<Temporal> OffsetDateTime::withOffsetSameInstant(int32_t newOffsetSeconds) const {
    const auto delta = static_cast<int64_t>(newOffsetSeconds) - offsetSeconds();
    auto shifted = plus(-delta, TemporalUnit::Seconds);
    return shifted->with(TemporalField::OffsetSeconds, newOffsetSeconds);
}

int OffsetDateTime::compareTo(const OffsetDateTime& other) const {
    auto thisLocal = toLocalDateTime();
    auto otherLocal = other.toLocalDateTime();
    const auto cmp = thisLocal->compareTo(*otherLocal);
    if (cmp != 0) {
        return cmp;
    }
    if (offsetSeconds() != other.offsetSeconds()) {
        return offsetSeconds() < other.offsetSeconds() ? -1 : 1;
    }
    return 0;
}

std::string OffsetDateTime::toIsoString() const {
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
    // auto secStr = std::to_string(sec);
    // if (sec < 10) {
    //     secStr = "0" + secStr;
    // }
    auto offset = hourStr + ":" + minStr;

    return toLocalDateTime()->toIsoString() + "+" + offset;
}

bool OffsetDateTime::isValidIsoString(const std::string& text) {
    return HHOffsetDateTime::isValidIsoString(text);
}

std::unique_ptr<OffsetDateTime> OffsetDateTime::parseIsoString(const std::string& text) {
    return std::make_unique<HHOffsetDateTime>(HHOffsetDateTime::parseIsoString(text));
}

std::unique_ptr<OffsetDateTime>
OffsetDateTime::fromTimePoint(const std::chrono::system_clock::time_point& timePoint,
                              int32_t offsetSeconds) {
    return std::make_unique<HHOffsetDateTime>(
        HHOffsetDateTime::fromTimePoint(timePoint, offsetSeconds));
}

} // namespace bas::time
