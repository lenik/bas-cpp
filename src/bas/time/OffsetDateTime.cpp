#include "OffsetDateTime.hpp"

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

} // namespace bas::time
