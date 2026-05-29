#include "Instant.hpp"

#include "HHDates.hpp"

#include <memory>

namespace bas::time {

int64_t Instant::toEpochMilli() const { return epochSecond() * 1000 + nano() / 1000000; }

std::unique_ptr<Temporal> Instant::truncatedTo(TemporalUnit /*unit*/) const { return nullptr; }

std::unique_ptr<Temporal> Instant::plusSeconds(int64_t seconds) const {
    return plus(seconds, TemporalUnit::Seconds);
}

std::unique_ptr<Temporal> Instant::plusMillis(int64_t millis) const {
    return plus(millis * 1000000, TemporalUnit::Nanos);
}

std::unique_ptr<Temporal> Instant::plusNanos(int64_t nanos) const {
    return plus(nanos, TemporalUnit::Nanos);
}

std::unique_ptr<Temporal> Instant::minusSeconds(int64_t seconds) const {
    return plusSeconds(-seconds);
}

std::unique_ptr<Temporal> Instant::minusMillis(int64_t millis) const { return plusMillis(-millis); }

std::unique_ptr<Temporal> Instant::minusNanos(int64_t nanos) const { return plusNanos(-nanos); }

int Instant::compareTo(const Instant& other) const {
    if (epochSecond() < other.epochSecond()) {
        return -1;
    }
    if (epochSecond() > other.epochSecond()) {
        return 1;
    }
    if (nano() < other.nano()) {
        return -1;
    }
    if (nano() > other.nano()) {
        return 1;
    }
    return 0;
}

bool Instant::isAfter(const Instant& other) const { return compareTo(other) > 0; }

bool Instant::isBefore(const Instant& other) const { return compareTo(other) < 0; }

bool Instant::isValidIsoString(const std::string& text) {
    return HHInstant::isValidIsoString(text);
}

std::string Instant::toIsoString() const {
    auto epoch = epochSecond();
    auto n = nano();
    return HHInstant::fromEpochSecond(epoch, n).toIsoString();
}

std::unique_ptr<Instant> Instant::parseIsoString(const std::string& text) {
    return std::make_unique<HHInstant>(HHInstant::parseIsoString(text));
}

} // namespace bas::time
