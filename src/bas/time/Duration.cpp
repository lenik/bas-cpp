#include "Duration.hpp"

namespace bas::time {

Duration::Duration() : m_seconds(0), m_nanos(0) {}

Duration::Duration(int64_t seconds, int32_t nanos) : m_seconds(seconds), m_nanos(nanos) {}

Duration Duration::ofSeconds(int64_t seconds) { return Duration(seconds, 0); }

Duration Duration::ofMillis(int64_t millis) {
    return Duration(millis / 1000, static_cast<int32_t>((millis % 1000) * 1000000));
}

Duration Duration::ofNanos(int64_t nanos) {
    return Duration(nanos / 1000000000LL, static_cast<int32_t>(nanos % 1000000000LL));
}

int64_t Duration::seconds() const { return m_seconds; }

int32_t Duration::nanos() const { return m_nanos; }

std::unique_ptr<Temporal> Duration::addTo(const Temporal& temporal) const {
    auto withSeconds = temporal.plus(m_seconds, TemporalUnit::Seconds);
    return withSeconds->plus(m_nanos, TemporalUnit::Nanos);
}

std::unique_ptr<Temporal> Duration::subtractFrom(const Temporal& temporal) const {
    auto minusSeconds = temporal.minus(m_seconds, TemporalUnit::Seconds);
    return minusSeconds->minus(m_nanos, TemporalUnit::Nanos);
}

} // namespace bas::time
