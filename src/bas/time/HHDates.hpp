#ifndef BAS_TIME_HH_DATES_HPP
#define BAS_TIME_HH_DATES_HPP

#include "Instant.hpp"
#include "LocalDate.hpp"
#include "LocalDateTime.hpp"
#include "LocalTime.hpp"
#include "OffsetDateTime.hpp"
#include "OffsetTime.hpp"
#include "ZonedDateTime.hpp"
#include "ZonedTime.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <date/date.h>
#include <date/tz.h>

namespace bas::time {

class HHInstant final : public Instant {
  public:
    using duration = std::chrono::nanoseconds;
    using sys_time = date::sys_time<duration>;

    HHInstant();
    explicit HHInstant(sys_time value);

    static HHInstant now();
    static HHInstant fromEpochSecond(int64_t epochSecond, int32_t nano = 0);
    static bool isValidIsoString(const std::string& text);
    static HHInstant parseIsoString(const std::string& text);

    const sys_time& value() const;

    int32_t nano() const override;
    int64_t epochSecond() const override;

    using Temporal::isSupported;
    bool isSupported(TemporalField field) const override;
    std::optional<int64_t> getLong(TemporalField field) const override;

    std::unique_ptr<Temporal> with(TemporalField field, int64_t newValue) const override;
    std::unique_ptr<Temporal> plus(int64_t amount, TemporalUnit unit) const override;

    std::string toIsoString() const override;

  private:
    sys_time m_value;
};

class HHLocalDate final : public LocalDate {
  public:
    HHLocalDate();
    explicit HHLocalDate(const date::local_days& value);
    HHLocalDate(int y, unsigned m, unsigned d);

    static HHLocalDate now();
    static bool isValidIsoString(const std::string& text);
    static HHLocalDate parseIsoString(const std::string& text);
    const date::local_days& value() const;

    int32_t year() const override;
    uint32_t month() const override;
    uint32_t day() const override;
    DayOfWeek dayOfWeek() const override;
    uint32_t dayOfYear() const override;
    bool isLeapYear() const override;
    uint32_t lengthOfMonth() const override;

    using Temporal::isSupported;
    bool isSupported(TemporalField field) const override;
    std::optional<int64_t> getLong(TemporalField field) const override;

    std::unique_ptr<Temporal> with(TemporalField field, int64_t newValue) const override;
    std::unique_ptr<Temporal> plus(int64_t amount, TemporalUnit unit) const override;

    std::string toIsoString() const override;

  private:
    date::local_days m_value;
};

class HHLocalTime final : public LocalTime {
  public:
    using duration = std::chrono::nanoseconds;

    HHLocalTime();
    explicit HHLocalTime(duration value);
    HHLocalTime(unsigned h, unsigned m, unsigned s, unsigned n = 0);

    static HHLocalTime now();
    static bool isValidIsoString(const std::string& text);
    static HHLocalTime parseIsoString(const std::string& text);
    duration value() const;

    uint32_t hour() const override;
    uint32_t minute() const override;
    uint32_t second() const override;
    uint32_t nano() const override;

    using Temporal::isSupported;
    bool isSupported(TemporalField field) const override;
    std::optional<int64_t> getLong(TemporalField field) const override;

    std::unique_ptr<Temporal> with(TemporalField field, int64_t newValue) const override;
    std::unique_ptr<Temporal> plus(int64_t amount, TemporalUnit unit) const override;

    std::string toIsoString() const override;

  private:
    duration m_value;
};

class HHLocalDateTime final : public LocalDateTime {
  public:
    using duration = std::chrono::nanoseconds;
    using local_time = date::local_time<duration>;

    HHLocalDateTime();
    explicit HHLocalDateTime(local_time value);
    HHLocalDateTime(const HHLocalDate& d, const HHLocalTime& t);

    static HHLocalDateTime now();
    static bool isValidIsoString(const std::string& text);
    static HHLocalDateTime parseIsoString(const std::string& text);
    const local_time& value() const;

    std::unique_ptr<LocalDate> toLocalDate() const override;
    std::unique_ptr<LocalTime> toLocalTime() const override;

    using Temporal::isSupported;
    bool isSupported(TemporalField field) const override;
    std::optional<int64_t> getLong(TemporalField field) const override;

    std::unique_ptr<Temporal> with(TemporalField field, int64_t newValue) const override;
    std::unique_ptr<Temporal> plus(int64_t amount, TemporalUnit unit) const override;

    std::string toIsoString() const override;

  private:
    local_time m_value;
};

class HHOffsetDateTime final : public OffsetDateTime {
  public:
    HHOffsetDateTime();
    HHOffsetDateTime(const HHLocalDateTime& localDateTime, int32_t offsetSeconds);
    
    static HHOffsetDateTime fromTimePoint(const std::chrono::system_clock::time_point& timePoint,
                                          int32_t offsetSeconds);
    
                                          static bool isValidIsoString(const std::string& text);
    static HHOffsetDateTime parseIsoString(const std::string& text);

    std::unique_ptr<LocalDateTime> toLocalDateTime() const override;
    int32_t offsetSeconds() const override;
    std::unique_ptr<ZoneOffset> offset() const override;

    using Temporal::isSupported;
    bool isSupported(TemporalField field) const override;
    std::optional<int64_t> getLong(TemporalField field) const override;

    std::unique_ptr<Temporal> with(TemporalField field, int64_t newValue) const override;
    std::unique_ptr<Temporal> plus(int64_t amount, TemporalUnit unit) const override;

    std::string toIsoString() const override;

  private:
    HHLocalDateTime m_localDateTime;
    int32_t m_offsetSeconds;
};

class HHOffsetTime final : public OffsetTime {
  public:
    HHOffsetTime();
    HHOffsetTime(const HHLocalTime& localTime, int32_t offsetSeconds);
    static bool isValidIsoString(const std::string& text);
    static HHOffsetTime parseIsoString(const std::string& text);

    std::unique_ptr<LocalTime> toLocalTime() const override;
    int32_t offsetSeconds() const override;
    std::unique_ptr<ZoneOffset> offset() const override;

    using Temporal::isSupported;
    bool isSupported(TemporalField field) const override;
    std::optional<int64_t> getLong(TemporalField field) const override;

    std::unique_ptr<Temporal> with(TemporalField field, int64_t newValue) const override;
    std::unique_ptr<Temporal> plus(int64_t amount, TemporalUnit unit) const override;

    std::string toIsoString() const override;

  private:
    HHLocalTime m_localTime;
    int32_t m_offsetSeconds;
};

class HHZonedDateTime final : public ZonedDateTime {
  public:
    using duration = std::chrono::nanoseconds;
    using zoned_time = date::zoned_time<duration>;

    HHZonedDateTime();
    explicit HHZonedDateTime(zoned_time value);

    static HHZonedDateTime now();
    static bool isValidIsoString(const std::string& text);
    static HHZonedDateTime parseIsoString(const std::string& text);
    const zoned_time& value() const;

    std::unique_ptr<OffsetDateTime> toOffsetDateTime() const override;
    std::string zoneId() const override;

    using Temporal::isSupported;
    bool isSupported(TemporalField field) const override;
    std::optional<int64_t> getLong(TemporalField field) const override;
    std::unique_ptr<Temporal> with(TemporalField field, int64_t newValue) const override;
    std::unique_ptr<Temporal> plus(int64_t amount, TemporalUnit unit) const override;

    std::string toIsoString() const override;

  private:
    zoned_time m_value;
};

class HHZonedTime final : public ZonedTime {
  public:
    HHZonedTime();
    HHZonedTime(const HHOffsetTime& offsetTime, std::string zoneId);
    static bool isValidIsoString(const std::string& text);
    static HHZonedTime parseIsoString(const std::string& text);

    std::unique_ptr<OffsetTime> toOffsetTime() const override;
    std::string zoneId() const override;

    using Temporal::isSupported;
    bool isSupported(TemporalField field) const override;
    std::optional<int64_t> getLong(TemporalField field) const override;
    std::unique_ptr<Temporal> with(TemporalField field, int64_t newValue) const override;
    std::unique_ptr<Temporal> plus(int64_t amount, TemporalUnit unit) const override;

    std::string toIsoString() const override;

  private:
    HHOffsetTime m_offsetTime;
    std::string m_zoneId;
};

} // namespace bas::time

#endif // BAS_TIME_HH_DATES_HPP