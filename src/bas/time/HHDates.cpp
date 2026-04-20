#include "HHDates.hpp"

#include "ZoneId.hpp"

namespace bas::time {

// ------------------------------------------------------------------------------------------------
// Instant
// ------------------------------------------------------------------------------------------------

HHInstant::HHInstant()
    : m_value(std::chrono::time_point_cast<duration>(std::chrono::system_clock::now())) {}

HHInstant::HHInstant(sys_time value) : m_value(value) {}

HHInstant HHInstant::now() { return HHInstant(); }

HHInstant HHInstant::fromEpochSecond(int64_t epochSecond, int32_t nano) {
    return HHInstant(sys_time(std::chrono::seconds(epochSecond) + std::chrono::nanoseconds(nano)));
}

const HHInstant::sys_time& HHInstant::value() const { return m_value; }

int32_t HHInstant::nano() const {
    const auto sec = std::chrono::duration_cast<std::chrono::seconds>(m_value.time_since_epoch());
    return static_cast<int32_t>((m_value.time_since_epoch() - sec).count());
}

int64_t HHInstant::epochSecond() const {
    return std::chrono::duration_cast<std::chrono::seconds>(m_value.time_since_epoch()).count();
}

bool HHInstant::isSupported(TemporalField field) const {
    return field == TemporalField::EpochSeconds || field == TemporalField::Nano;
}

std::optional<int64_t> HHInstant::getLong(TemporalField field) const {
    if (field == TemporalField::EpochSeconds) {
        return epochSecond();
    }
    if (field == TemporalField::Nano) {
        return nano();
    }
    return std::nullopt;
}

std::unique_ptr<Temporal> HHInstant::with(TemporalField field, int64_t newValue) const {
    if (field == TemporalField::EpochSeconds) {
        return std::make_unique<HHInstant>(fromEpochSecond(newValue, nano()));
    }
    if (field == TemporalField::Nano) {
        return std::make_unique<HHInstant>(
            fromEpochSecond(epochSecond(), static_cast<int32_t>(newValue)));
    }
    return std::make_unique<HHInstant>(*this);
}

std::unique_ptr<Temporal> HHInstant::plus(int64_t amount, TemporalUnit unit) const {
    switch (unit) {
    case TemporalUnit::Nanos:
        return std::make_unique<HHInstant>(m_value + std::chrono::nanoseconds(amount));
    case TemporalUnit::Seconds:
        return std::make_unique<HHInstant>(m_value + std::chrono::seconds(amount));
    case TemporalUnit::Minutes:
        return std::make_unique<HHInstant>(m_value + std::chrono::minutes(amount));
    case TemporalUnit::Hours:
        return std::make_unique<HHInstant>(m_value + std::chrono::hours(amount));
    case TemporalUnit::Days:
        return std::make_unique<HHInstant>(m_value + std::chrono::hours(24 * amount));
    default:
        return std::make_unique<HHInstant>(*this);
    }
}

std::string HHInstant::toIsoString() const { return date::format("%FT%TZ", m_value); }

// ------------------------------------------------------------------------------------------------
// LocalDate
// ------------------------------------------------------------------------------------------------

HHLocalDate::HHLocalDate()
    : m_value(date::floor<date::days>(
          date::make_zoned(date::current_zone(), std::chrono::system_clock::now())
              .get_local_time())) {}

HHLocalDate::HHLocalDate(const date::local_days& value) : m_value(value) {}

HHLocalDate::HHLocalDate(int y, unsigned m, unsigned d)
    : m_value(date::local_days(date::year(y) / m / d)) {}

HHLocalDate HHLocalDate::now() { return HHLocalDate(); }

const date::local_days& HHLocalDate::value() const { return m_value; }

int32_t HHLocalDate::year() const {
    return static_cast<int32_t>(date::year_month_day(m_value).year());
}

uint32_t HHLocalDate::month() const {
    return static_cast<uint32_t>(unsigned(date::year_month_day(m_value).month()));
}

uint32_t HHLocalDate::day() const {
    return static_cast<uint32_t>(unsigned(date::year_month_day(m_value).day()));
}

DayOfWeek HHLocalDate::dayOfWeek() const {
    const auto iso = date::weekday(m_value).iso_encoding();
    return static_cast<DayOfWeek>(iso);
}

uint32_t HHLocalDate::dayOfYear() const {
    const auto ymd = date::year_month_day(m_value);
    const auto start = date::local_days(ymd.year() / 1 / 1);
    return static_cast<uint32_t>((m_value - start).count()) + 1;
}

bool HHLocalDate::isLeapYear() const { return static_cast<bool>(date::year(year()).is_leap()); }

uint32_t HHLocalDate::lengthOfMonth() const {
    const auto ymd = date::year_month_day(m_value);
    const auto nextMonth = date::year_month_day(date::local_days(ymd + date::months(1)));
    return static_cast<uint32_t>((date::local_days(nextMonth.year() / nextMonth.month() / 1) -
                                  date::local_days(ymd.year() / ymd.month() / 1))
                                     .count());
}

bool HHLocalDate::isSupported(TemporalField field) const {
    return field == TemporalField::Year || field == TemporalField::Month ||
           field == TemporalField::Day || field == TemporalField::DayOfYear ||
           field == TemporalField::DayOfWeek;
}

std::optional<int64_t> HHLocalDate::getLong(TemporalField field) const {
    if (field == TemporalField::Year) {
        return year();
    }
    if (field == TemporalField::Month) {
        return month();
    }
    if (field == TemporalField::Day) {
        return day();
    }
    if (field == TemporalField::DayOfYear) {
        return dayOfYear();
    }
    if (field == TemporalField::DayOfWeek) {
        return static_cast<int64_t>(dayOfWeek());
    }
    return std::nullopt;
}

std::unique_ptr<Temporal> HHLocalDate::with(TemporalField field, int64_t newValue) const {
    auto ymd = date::year_month_day(m_value);
    if (field == TemporalField::Year) {
        ymd = date::year(static_cast<int>(newValue)) / ymd.month() / ymd.day();
    } else if (field == TemporalField::Month) {
        ymd = ymd.year() / static_cast<unsigned>(newValue) / ymd.day();
    } else if (field == TemporalField::Day) {
        ymd = ymd.year() / ymd.month() / static_cast<unsigned>(newValue);
    }
    return std::make_unique<HHLocalDate>(date::local_days(ymd));
}

std::unique_ptr<Temporal> HHLocalDate::plus(int64_t amount, TemporalUnit unit) const {
    switch (unit) {
    case TemporalUnit::Days:
        return std::make_unique<HHLocalDate>(m_value + date::days(amount));
    case TemporalUnit::Weeks:
        return std::make_unique<HHLocalDate>(m_value + date::days(amount * 7));
    case TemporalUnit::Months:
        return std::make_unique<HHLocalDate>(
            date::local_days(date::year_month_day(m_value) + date::months(amount)));
    case TemporalUnit::Years:
        return std::make_unique<HHLocalDate>(
            date::local_days(date::year_month_day(m_value) + date::years(amount)));
    default:
        return std::make_unique<HHLocalDate>(*this);
    }
}

std::string HHLocalDate::toIsoString() const { return date::format("%F", m_value); }

// ------------------------------------------------------------------------------------------------
// LocalDateTime
// ------------------------------------------------------------------------------------------------

HHLocalDateTime::HHLocalDateTime()
    : m_value(date::floor<duration>(
          date::make_zoned(date::current_zone(), std::chrono::system_clock::now())
              .get_local_time())) {}

HHLocalDateTime::HHLocalDateTime(local_time value) : m_value(value) {}

HHLocalDateTime::HHLocalDateTime(const HHLocalDate& d, const HHLocalTime& t)
    : m_value(d.value() + t.value()) {}

HHLocalDateTime HHLocalDateTime::now() { return HHLocalDateTime(); }

const HHLocalDateTime::local_time& HHLocalDateTime::value() const { return m_value; }

std::unique_ptr<LocalDate> HHLocalDateTime::toLocalDate() const {
    return std::make_unique<HHLocalDate>(date::floor<date::days>(m_value));
}

std::unique_ptr<LocalTime> HHLocalDateTime::toLocalTime() const {
    return std::make_unique<HHLocalTime>(
        std::chrono::duration_cast<duration>(m_value - date::floor<date::days>(m_value)));
}

bool HHLocalDateTime::isSupported(TemporalField field) const {
    return field == TemporalField::Year || field == TemporalField::Month ||
           field == TemporalField::Day || field == TemporalField::DayOfYear ||
           field == TemporalField::DayOfWeek || field == TemporalField::Hour ||
           field == TemporalField::Minute || field == TemporalField::Second ||
           field == TemporalField::Nano;
}

std::optional<int64_t> HHLocalDateTime::getLong(TemporalField field) const {
    if (field == TemporalField::Year || field == TemporalField::Month ||
        field == TemporalField::Day || field == TemporalField::DayOfYear ||
        field == TemporalField::DayOfWeek) {
        return toLocalDate()->getLong(field);
    }
    if (field == TemporalField::Hour || field == TemporalField::Minute ||
        field == TemporalField::Second || field == TemporalField::Nano) {
        return toLocalTime()->getLong(field);
    }
    return std::nullopt;
}

std::unique_ptr<Temporal> HHLocalDateTime::with(TemporalField field, int64_t newValue) const {
    if (field == TemporalField::Year || field == TemporalField::Month ||
        field == TemporalField::Day) {
        auto datePart = toLocalDate()->with(field, newValue);
        return std::make_unique<HHLocalDateTime>(*static_cast<HHLocalDate*>(datePart.get()),
                                                 *static_cast<HHLocalTime*>(toLocalTime().get()));
    }
    if (field == TemporalField::Hour || field == TemporalField::Minute ||
        field == TemporalField::Second || field == TemporalField::Nano) {
        auto timePart = toLocalTime()->with(field, newValue);
        return std::make_unique<HHLocalDateTime>(*static_cast<HHLocalDate*>(toLocalDate().get()),
                                                 *static_cast<HHLocalTime*>(timePart.get()));
    }
    return std::make_unique<HHLocalDateTime>(*this);
}

std::unique_ptr<Temporal> HHLocalDateTime::plus(int64_t amount, TemporalUnit unit) const {
    switch (unit) {
    case TemporalUnit::Nanos:
        return std::make_unique<HHLocalDateTime>(m_value + std::chrono::nanoseconds(amount));
    case TemporalUnit::Seconds:
        return std::make_unique<HHLocalDateTime>(m_value + std::chrono::seconds(amount));
    case TemporalUnit::Minutes:
        return std::make_unique<HHLocalDateTime>(m_value + std::chrono::minutes(amount));
    case TemporalUnit::Hours:
        return std::make_unique<HHLocalDateTime>(m_value + std::chrono::hours(amount));
    case TemporalUnit::Days:
        return std::make_unique<HHLocalDateTime>(m_value + date::days(amount));
    case TemporalUnit::Weeks:
        return std::make_unique<HHLocalDateTime>(m_value + date::days(amount * 7));
    default:
        return std::make_unique<HHLocalDateTime>(*this);
    }
}

std::string HHLocalDateTime::toIsoString() const { return date::format("%FT%T", m_value); }

// ------------------------------------------------------------------------------------------------
// LocalTime
// ------------------------------------------------------------------------------------------------

HHLocalTime::HHLocalTime() {
    const auto now =
        std::chrono::time_point_cast<duration>(std::chrono::system_clock::now()).time_since_epoch();
    m_value = std::chrono::duration_cast<duration>(now % std::chrono::hours(24));
}

HHLocalTime::HHLocalTime(duration value) : m_value(value) {}

HHLocalTime::HHLocalTime(unsigned h, unsigned m, unsigned s, unsigned n)
    : m_value(std::chrono::hours(h) + std::chrono::minutes(m) + std::chrono::seconds(s) +
              std::chrono::nanoseconds(n)) {}

HHLocalTime HHLocalTime::now() { return HHLocalTime(); }

HHLocalTime::duration HHLocalTime::value() const { return m_value; }

uint32_t HHLocalTime::hour() const {
    return static_cast<uint32_t>(date::hh_mm_ss<duration>(m_value).hours().count());
}

uint32_t HHLocalTime::minute() const {
    return static_cast<uint32_t>(date::hh_mm_ss<duration>(m_value).minutes().count());
}

uint32_t HHLocalTime::second() const {
    return static_cast<uint32_t>(date::hh_mm_ss<duration>(m_value).seconds().count());
}

uint32_t HHLocalTime::nano() const {
    const auto hms = date::hh_mm_ss<duration>(m_value);
    return static_cast<uint32_t>(hms.subseconds().count());
}

bool HHLocalTime::isSupported(TemporalField field) const {
    return field == TemporalField::Hour || field == TemporalField::Minute ||
           field == TemporalField::Second || field == TemporalField::Nano;
}

std::optional<int64_t> HHLocalTime::getLong(TemporalField field) const {
    if (field == TemporalField::Hour) {
        return hour();
    }
    if (field == TemporalField::Minute) {
        return minute();
    }
    if (field == TemporalField::Second) {
        return second();
    }
    if (field == TemporalField::Nano) {
        return nano();
    }
    return std::nullopt;
}

std::unique_ptr<Temporal> HHLocalTime::with(TemporalField field, int64_t newValue) const {
    auto h = hour();
    auto m = minute();
    auto s = second();
    auto n = nano();
    if (field == TemporalField::Hour) {
        h = static_cast<uint32_t>(newValue);
    } else if (field == TemporalField::Minute) {
        m = static_cast<uint32_t>(newValue);
    } else if (field == TemporalField::Second) {
        s = static_cast<uint32_t>(newValue);
    } else if (field == TemporalField::Nano) {
        n = static_cast<uint32_t>(newValue);
    }
    return std::make_unique<HHLocalTime>(h, m, s, n);
}

std::unique_ptr<Temporal> HHLocalTime::plus(int64_t amount, TemporalUnit unit) const {
    switch (unit) {
    case TemporalUnit::Nanos:
        return std::make_unique<HHLocalTime>(m_value + std::chrono::nanoseconds(amount));
    case TemporalUnit::Seconds:
        return std::make_unique<HHLocalTime>(m_value + std::chrono::seconds(amount));
    case TemporalUnit::Minutes:
        return std::make_unique<HHLocalTime>(m_value + std::chrono::minutes(amount));
    case TemporalUnit::Hours:
        return std::make_unique<HHLocalTime>(m_value + std::chrono::hours(amount));
    default:
        return std::make_unique<HHLocalTime>(*this);
    }
}

std::string HHLocalTime::toIsoString() const { return date::format("%T", m_value); }

// ------------------------------------------------------------------------------------------------
// OffsetDateTime
// ------------------------------------------------------------------------------------------------

HHOffsetDateTime::HHOffsetDateTime()
    : m_localDateTime(HHLocalDateTime::now()), m_offsetSeconds(0) {}

HHOffsetDateTime::HHOffsetDateTime(const HHLocalDateTime& localDateTime, int32_t offsetSeconds)
    : m_localDateTime(localDateTime), m_offsetSeconds(offsetSeconds) {}

std::unique_ptr<LocalDateTime> HHOffsetDateTime::toLocalDateTime() const {
    return std::make_unique<HHLocalDateTime>(m_localDateTime);
}

int32_t HHOffsetDateTime::offsetSeconds() const { return m_offsetSeconds; }

std::unique_ptr<ZoneOffset> HHOffsetDateTime::offset() const {
    return std::make_unique<ZoneOffset>(m_offsetSeconds);
}

bool HHOffsetDateTime::isSupported(TemporalField field) const {
    return m_localDateTime.isSupported(field) || field == TemporalField::OffsetSeconds;
}

std::optional<int64_t> HHOffsetDateTime::getLong(TemporalField field) const {
    if (field == TemporalField::OffsetSeconds) {
        return m_offsetSeconds;
    }
    return m_localDateTime.getLong(field);
}

std::unique_ptr<Temporal> HHOffsetDateTime::with(TemporalField field, int64_t newValue) const {
    if (field == TemporalField::OffsetSeconds) {
        return std::make_unique<HHOffsetDateTime>(m_localDateTime, static_cast<int32_t>(newValue));
    }
    auto t = m_localDateTime.with(field, newValue);
    return std::make_unique<HHOffsetDateTime>(*static_cast<HHLocalDateTime*>(t.get()),
                                              m_offsetSeconds);
}

std::unique_ptr<Temporal> HHOffsetDateTime::plus(int64_t amount, TemporalUnit unit) const {
    auto t = m_localDateTime.plus(amount, unit);
    return std::make_unique<HHOffsetDateTime>(*static_cast<HHLocalDateTime*>(t.get()),
                                              m_offsetSeconds);
}

std::string HHOffsetDateTime::toIsoString() const {
    const auto sign = m_offsetSeconds < 0 ? '-' : '+';
    const auto absOff = m_offsetSeconds < 0 ? -m_offsetSeconds : m_offsetSeconds;
    const auto hh = absOff / 3600;
    const auto mm = (absOff % 3600) / 60;
    return m_localDateTime.toIsoString() + sign + (hh < 10 ? "0" : "") + std::to_string(hh) + ":" +
           (mm < 10 ? "0" : "") + std::to_string(mm);
}

// ------------------------------------------------------------------------------------------------
// OffsetTime
// ------------------------------------------------------------------------------------------------

HHOffsetTime::HHOffsetTime() : m_localTime(HHLocalTime::now()), m_offsetSeconds(0) {}

HHOffsetTime::HHOffsetTime(const HHLocalTime& localTime, int32_t offsetSeconds)
    : m_localTime(localTime), m_offsetSeconds(offsetSeconds) {}

std::unique_ptr<LocalTime> HHOffsetTime::toLocalTime() const {
    return std::make_unique<HHLocalTime>(m_localTime);
}

int32_t HHOffsetTime::offsetSeconds() const { return m_offsetSeconds; }

std::unique_ptr<ZoneOffset> HHOffsetTime::offset() const {
    return std::make_unique<ZoneOffset>(m_offsetSeconds);
}

bool HHOffsetTime::isSupported(TemporalField field) const {
    return m_localTime.isSupported(field) || field == TemporalField::OffsetSeconds;
}

std::optional<int64_t> HHOffsetTime::getLong(TemporalField field) const {
    if (field == TemporalField::OffsetSeconds) {
        return m_offsetSeconds;
    }
    return m_localTime.getLong(field);
}

std::unique_ptr<Temporal> HHOffsetTime::with(TemporalField field, int64_t newValue) const {
    if (field == TemporalField::OffsetSeconds) {
        return std::make_unique<HHOffsetTime>(m_localTime, static_cast<int32_t>(newValue));
    }
    auto t = m_localTime.with(field, newValue);
    return std::make_unique<HHOffsetTime>(*static_cast<HHLocalTime*>(t.get()), m_offsetSeconds);
}

std::unique_ptr<Temporal> HHOffsetTime::plus(int64_t amount, TemporalUnit unit) const {
    auto t = m_localTime.plus(amount, unit);
    return std::make_unique<HHOffsetTime>(*static_cast<HHLocalTime*>(t.get()), m_offsetSeconds);
}

std::string HHOffsetTime::toIsoString() const {
    const auto sign = m_offsetSeconds < 0 ? '-' : '+';
    const auto absOff = m_offsetSeconds < 0 ? -m_offsetSeconds : m_offsetSeconds;
    const auto hh = absOff / 3600;
    const auto mm = (absOff % 3600) / 60;
    return m_localTime.toIsoString() + sign + (hh < 10 ? "0" : "") + std::to_string(hh) + ":" +
           (mm < 10 ? "0" : "") + std::to_string(mm);
}

// ------------------------------------------------------------------------------------------------
// ZonedDateTime
// ------------------------------------------------------------------------------------------------

HHZonedDateTime::HHZonedDateTime()
    : m_value(date::current_zone(),
              std::chrono::time_point_cast<duration>(std::chrono::system_clock::now())) {}

HHZonedDateTime::HHZonedDateTime(zoned_time value) : m_value(std::move(value)) {}

HHZonedDateTime HHZonedDateTime::now() { return HHZonedDateTime(); }

const HHZonedDateTime::zoned_time& HHZonedDateTime::value() const { return m_value; }

std::unique_ptr<OffsetDateTime> HHZonedDateTime::toOffsetDateTime() const {
    const auto local = date::floor<duration>(m_value.get_local_time());
    const auto info = m_value.get_info();
    return std::make_unique<HHOffsetDateTime>(HHLocalDateTime(local),
                                              static_cast<int32_t>(info.offset.count()));
}

std::string HHZonedDateTime::zoneId() const { return m_value.get_time_zone()->name(); }

bool HHZonedDateTime::isSupported(TemporalField field) const {
    return toOffsetDateTime()->isSupported(field);
}

std::optional<int64_t> HHZonedDateTime::getLong(TemporalField field) const {
    return toOffsetDateTime()->getLong(field);
}

std::unique_ptr<Temporal> HHZonedDateTime::with(TemporalField field, int64_t newValue) const {
    auto odt = toOffsetDateTime()->with(field, newValue);
    const auto* hhOdt = static_cast<HHOffsetDateTime*>(odt.get());
    auto local = static_cast<HHLocalDateTime*>(hhOdt->toLocalDateTime().get())->value();
    auto sys = date::make_zoned(zoneId(), local).get_sys_time();
    return std::make_unique<HHZonedDateTime>(zoned_time(date::locate_zone(zoneId()), sys));
}

std::unique_ptr<Temporal> HHZonedDateTime::plus(int64_t amount, TemporalUnit unit) const {
    switch (unit) {
    case TemporalUnit::Nanos:
        return std::make_unique<HHZonedDateTime>(zoned_time(
            m_value.get_time_zone(), m_value.get_sys_time() + std::chrono::nanoseconds(amount)));
    case TemporalUnit::Seconds:
        return std::make_unique<HHZonedDateTime>(zoned_time(
            m_value.get_time_zone(), m_value.get_sys_time() + std::chrono::seconds(amount)));
    case TemporalUnit::Minutes:
        return std::make_unique<HHZonedDateTime>(zoned_time(
            m_value.get_time_zone(), m_value.get_sys_time() + std::chrono::minutes(amount)));
    case TemporalUnit::Hours:
        return std::make_unique<HHZonedDateTime>(zoned_time(
            m_value.get_time_zone(), m_value.get_sys_time() + std::chrono::hours(amount)));
    case TemporalUnit::Days:
        return std::make_unique<HHZonedDateTime>(
            zoned_time(m_value.get_time_zone(), m_value.get_sys_time() + date::days(amount)));
    default:
        return std::make_unique<HHZonedDateTime>(*this);
    }
}

std::string HHZonedDateTime::toIsoString() const { return date::format("%FT%T%Ez", m_value); }

// ------------------------------------------------------------------------------------------------
// ZonedTime
// ------------------------------------------------------------------------------------------------

HHZonedTime::HHZonedTime() : m_offsetTime(), m_zoneId("UTC") {}

HHZonedTime::HHZonedTime(const HHOffsetTime& offsetTime, std::string zoneId)
    : m_offsetTime(offsetTime), m_zoneId(std::move(zoneId)) {}

std::unique_ptr<OffsetTime> HHZonedTime::toOffsetTime() const {
    return std::make_unique<HHOffsetTime>(m_offsetTime);
}

std::string HHZonedTime::zoneId() const { return m_zoneId; }

bool HHZonedTime::isSupported(TemporalField field) const { return m_offsetTime.isSupported(field); }

std::optional<int64_t> HHZonedTime::getLong(TemporalField field) const {
    return m_offsetTime.getLong(field);
}

std::unique_ptr<Temporal> HHZonedTime::with(TemporalField field, int64_t newValue) const {
    auto t = m_offsetTime.with(field, newValue);
    return std::make_unique<HHZonedTime>(*static_cast<HHOffsetTime*>(t.get()), m_zoneId);
}

std::unique_ptr<Temporal> HHZonedTime::plus(int64_t amount, TemporalUnit unit) const {
    auto t = m_offsetTime.plus(amount, unit);
    return std::make_unique<HHZonedTime>(*static_cast<HHOffsetTime*>(t.get()), m_zoneId);
}

std::string HHZonedTime::toIsoString() const {
    return m_offsetTime.toIsoString() + "[" + m_zoneId + "]";
}

} // namespace bas::time
