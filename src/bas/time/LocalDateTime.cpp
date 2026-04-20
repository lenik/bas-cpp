#include "LocalDateTime.hpp"

namespace bas::time {

int32_t LocalDateTime::year() const { return toLocalDate()->year(); }

uint32_t LocalDateTime::month() const { return toLocalDate()->month(); }

uint32_t LocalDateTime::day() const { return toLocalDate()->day(); }

uint32_t LocalDateTime::hour() const { return toLocalTime()->hour(); }

uint32_t LocalDateTime::minute() const { return toLocalTime()->minute(); }

uint32_t LocalDateTime::second() const { return toLocalTime()->second(); }

uint32_t LocalDateTime::nano() const { return toLocalTime()->nano(); }

std::unique_ptr<Temporal> LocalDateTime::withYear(int32_t y) const {
    return with(TemporalField::Year, y);
}

std::unique_ptr<Temporal> LocalDateTime::withMonth(uint32_t m) const {
    return with(TemporalField::Month, m);
}

std::unique_ptr<Temporal> LocalDateTime::withDayOfMonth(uint32_t d) const {
    return with(TemporalField::Day, d);
}

std::unique_ptr<Temporal> LocalDateTime::withHour(uint32_t h) const {
    return with(TemporalField::Hour, h);
}

std::unique_ptr<Temporal> LocalDateTime::withMinute(uint32_t m) const {
    return with(TemporalField::Minute, m);
}

std::unique_ptr<Temporal> LocalDateTime::withSecond(uint32_t s) const {
    return with(TemporalField::Second, s);
}

std::unique_ptr<Temporal> LocalDateTime::withNano(uint32_t n) const {
    return with(TemporalField::Nano, n);
}

std::unique_ptr<Temporal> LocalDateTime::plusYears(int64_t years) const {
    return plus(years, TemporalUnit::Years);
}

std::unique_ptr<Temporal> LocalDateTime::plusMonths(int64_t months) const {
    return plus(months, TemporalUnit::Months);
}

std::unique_ptr<Temporal> LocalDateTime::plusWeeks(int64_t weeks) const {
    return plus(weeks, TemporalUnit::Weeks);
}

std::unique_ptr<Temporal> LocalDateTime::plusDays(int64_t days) const {
    return plus(days, TemporalUnit::Days);
}

std::unique_ptr<Temporal> LocalDateTime::plusHours(int64_t hours) const {
    return plus(hours, TemporalUnit::Hours);
}

std::unique_ptr<Temporal> LocalDateTime::plusMinutes(int64_t minutes) const {
    return plus(minutes, TemporalUnit::Minutes);
}

std::unique_ptr<Temporal> LocalDateTime::plusSeconds(int64_t seconds) const {
    return plus(seconds, TemporalUnit::Seconds);
}

std::unique_ptr<Temporal> LocalDateTime::plusNanos(int64_t nanos) const {
    return plus(nanos, TemporalUnit::Nanos);
}

int LocalDateTime::compareTo(const LocalDateTime& other) const {
    const auto thisDate = toLocalDate();
    const auto otherDate = other.toLocalDate();
    const auto dateCmp = thisDate->compareTo(*otherDate);
    if (dateCmp != 0) {
        return dateCmp;
    }
    return toLocalTime()->compareTo(*other.toLocalTime());
}

bool LocalDateTime::isAfter(const LocalDateTime& other) const { return compareTo(other) > 0; }

bool LocalDateTime::isBefore(const LocalDateTime& other) const { return compareTo(other) < 0; }

} // namespace bas::time
