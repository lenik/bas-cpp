#include "LocalDate.hpp"

namespace bas::time {

DayOfWeek LocalDate::dayOfWeek() const { return DayOfWeek::Monday; }

uint32_t LocalDate::dayOfYear() const { return 1; }

bool LocalDate::isLeapYear() const { return false; }

uint32_t LocalDate::lengthOfMonth() const { return 31; }

uint32_t LocalDate::lengthOfYear() const { return isLeapYear() ? 366 : 365; }

std::unique_ptr<Temporal> LocalDate::withYear(int32_t y) const {
    return with(TemporalField::Year, y);
}

std::unique_ptr<Temporal> LocalDate::withMonth(uint32_t m) const {
    return with(TemporalField::Month, m);
}

std::unique_ptr<Temporal> LocalDate::withDayOfMonth(uint32_t d) const {
    return with(TemporalField::Day, d);
}

std::unique_ptr<Temporal> LocalDate::plusYears(int64_t years) const {
    return plus(years, TemporalUnit::Years);
}

std::unique_ptr<Temporal> LocalDate::plusMonths(int64_t months) const {
    return plus(months, TemporalUnit::Months);
}

std::unique_ptr<Temporal> LocalDate::plusWeeks(int64_t weeks) const {
    return plus(weeks, TemporalUnit::Weeks);
}

std::unique_ptr<Temporal> LocalDate::plusDays(int64_t days) const {
    return plus(days, TemporalUnit::Days);
}

std::unique_ptr<Temporal> LocalDate::minusYears(int64_t years) const { return plusYears(-years); }

std::unique_ptr<Temporal> LocalDate::minusMonths(int64_t months) const {
    return plusMonths(-months);
}

std::unique_ptr<Temporal> LocalDate::minusWeeks(int64_t weeks) const { return plusWeeks(-weeks); }

std::unique_ptr<Temporal> LocalDate::minusDays(int64_t days) const { return plusDays(-days); }

int LocalDate::compareTo(const LocalDate& other) const {
    if (year() != other.year()) {
        return year() < other.year() ? -1 : 1;
    }
    if (month() != other.month()) {
        return month() < other.month() ? -1 : 1;
    }
    if (day() != other.day()) {
        return day() < other.day() ? -1 : 1;
    }
    return 0;
}

bool LocalDate::isAfter(const LocalDate& other) const { return compareTo(other) > 0; }

bool LocalDate::isBefore(const LocalDate& other) const { return compareTo(other) < 0; }

} // namespace bas::time
