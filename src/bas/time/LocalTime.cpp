#include "LocalTime.hpp"

namespace bas::time {

std::unique_ptr<Temporal> LocalTime::withHour(uint32_t h) const {
    return with(TemporalField::Hour, h);
}

std::unique_ptr<Temporal> LocalTime::withMinute(uint32_t m) const {
    return with(TemporalField::Minute, m);
}

std::unique_ptr<Temporal> LocalTime::withSecond(uint32_t s) const {
    return with(TemporalField::Second, s);
}

std::unique_ptr<Temporal> LocalTime::withNano(uint32_t n) const {
    return with(TemporalField::Nano, n);
}

std::unique_ptr<Temporal> LocalTime::plusHours(int64_t hours) const {
    return plus(hours, TemporalUnit::Hours);
}

std::unique_ptr<Temporal> LocalTime::plusMinutes(int64_t minutes) const {
    return plus(minutes, TemporalUnit::Minutes);
}

std::unique_ptr<Temporal> LocalTime::plusSeconds(int64_t seconds) const {
    return plus(seconds, TemporalUnit::Seconds);
}

std::unique_ptr<Temporal> LocalTime::plusNanos(int64_t nanos) const {
    return plus(nanos, TemporalUnit::Nanos);
}

std::unique_ptr<Temporal> LocalTime::minusHours(int64_t hours) const { return plusHours(-hours); }

std::unique_ptr<Temporal> LocalTime::minusMinutes(int64_t minutes) const {
    return plusMinutes(-minutes);
}

std::unique_ptr<Temporal> LocalTime::minusSeconds(int64_t seconds) const {
    return plusSeconds(-seconds);
}

std::unique_ptr<Temporal> LocalTime::minusNanos(int64_t nanos) const { return plusNanos(-nanos); }

int LocalTime::compareTo(const LocalTime& other) const {
    if (hour() != other.hour()) {
        return hour() < other.hour() ? -1 : 1;
    }
    if (minute() != other.minute()) {
        return minute() < other.minute() ? -1 : 1;
    }
    if (second() != other.second()) {
        return second() < other.second() ? -1 : 1;
    }
    if (nano() != other.nano()) {
        return nano() < other.nano() ? -1 : 1;
    }
    return 0;
}

bool LocalTime::isAfter(const LocalTime& other) const { return compareTo(other) > 0; }

bool LocalTime::isBefore(const LocalTime& other) const { return compareTo(other) < 0; }

int64_t LocalTime::toSecondOfDay() const {
    return static_cast<int64_t>(hour()) * 3600 + minute() * 60 + second();
}

int64_t LocalTime::toNanoOfDay() const { return toSecondOfDay() * 1000000000LL + nano(); }

} // namespace bas::time
