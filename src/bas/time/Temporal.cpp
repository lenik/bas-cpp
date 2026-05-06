#include "Temporal.hpp"

#include "TemporalAccessor.hpp"

#include <chrono>
#include <ctime>
#include <string>

namespace bas::time {

bool Temporal::isSupported(TemporalUnit /*unit*/) const { return false; }

std::unique_ptr<Temporal> Temporal::with(const TemporalAdjuster& adjuster) const {
    return adjuster.adjustInto(*this);
}

std::unique_ptr<Temporal> Temporal::plus(const TemporalAmount& amount) const {
    return amount.addTo(*this);
}

std::unique_ptr<Temporal> Temporal::minus(const TemporalAmount& amount) const {
    return amount.subtractFrom(*this);
}

std::unique_ptr<Temporal> Temporal::minus(int64_t amount, TemporalUnit unit) const {
    return plus(-amount, unit);
}

std::optional<int64_t> Temporal::until(const Temporal& /*endExclusive*/,
                                       TemporalUnit /*unit*/) const {
    return std::nullopt;
}

int64_t Temporal::epochNano(int64_t defaultValue) const {
    auto second = getLong(TemporalField::EpochSeconds);
    auto nano = getLong(TemporalField::Nano);
    if (second.has_value() && nano.has_value()) {
        return second.value() * 1000000000 + nano.value();
    }
    return defaultValue;
}

int64_t Temporal::epochMilli(int64_t defaultValue) const {
    auto second = getLong(TemporalField::EpochSeconds);
    auto nano = getLong(TemporalField::Nano);
    if (second.has_value() && nano.has_value()) {
        return second.value() * 1000 + nano.value() / 1000000;
    }
    return defaultValue;
}

int64_t Temporal::epochSecond(int64_t defaultValue) const {
    auto second = getLong(TemporalField::EpochSeconds);
    if (second.has_value()) {
        return second.value();
    }
    return defaultValue;
}

int Temporal::year(int defaultValue) const { return get(TemporalField::Year, defaultValue); }

int Temporal::month(int defaultValue) const { return get(TemporalField::Month, defaultValue); }

int Temporal::day(int defaultValue) const { return get(TemporalField::Day, defaultValue); }

int Temporal::dayOfWeek(int defaultValue) const {
    return get(TemporalField::DayOfWeek, defaultValue);
}

int Temporal::dayOfYear(int defaultValue) const {
    return get(TemporalField::DayOfYear, defaultValue);
}

int Temporal::hour(int defaultValue) const { return get(TemporalField::Hour, defaultValue); }

int Temporal::minute(int defaultValue) const { return get(TemporalField::Minute, defaultValue); }

int Temporal::second(int defaultValue) const { return get(TemporalField::Second, defaultValue); }

int Temporal::nano(int defaultValue) const { return get(TemporalField::Nano, defaultValue); }

int Temporal::offsetSeconds(int defaultValue) const {
    return get(TemporalField::OffsetSeconds, defaultValue);
}

std::chrono::time_point<std::chrono::system_clock> Temporal::toTimePoint() const {
    auto nano = epochNano(0);
    std::chrono::system_clock::duration dur(nano);
    return std::chrono::system_clock::time_point(dur);
}

time_t Temporal::toTime() const {
    auto tm = toTm();
    return mktime(&tm);
}

struct tm Temporal::toTm() const {
    struct tm tm;
    tm.tm_sec = second(0);
    tm.tm_min = minute(0);
    tm.tm_hour = hour(0);
    tm.tm_mday = day(0);
    tm.tm_mon = month(0) - 1;
    tm.tm_year = year(0) - 1900;
    tm.tm_wday = dayOfWeek(0) % 7; // 0 for sunday
    tm.tm_yday = dayOfYear(0) - 1;
    tm.tm_isdst = -1;
    return tm;
}

std::filesystem::file_time_type Temporal::toFileTime() const {
    auto tp = toTimePoint();
    return std::filesystem::file_time_type::clock::time_point(tp.time_since_epoch());
}

std::string Temporal::toIsoString() const { return std::to_string(epochSecond(0)); }

} // namespace bas::time