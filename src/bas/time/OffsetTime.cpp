#include "OffsetTime.hpp"

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

} // namespace bas::time
