#ifndef BAS_TIME_LOCAL_DATE_TIME_HPP
#define BAS_TIME_LOCAL_DATE_TIME_HPP

#include "LocalDate.hpp"
#include "LocalTime.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace bas::time {

class ZoneOffset;
class OffsetDateTime;
class ZonedDateTime;

class LocalDateTime : public Temporal {
public:
    ~LocalDateTime() override = default;

    virtual std::unique_ptr<LocalDate> toLocalDate() const = 0;
    virtual std::unique_ptr<LocalTime> toLocalTime() const = 0;
    virtual int32_t year() const;
    virtual uint32_t month() const;
    virtual uint32_t day() const;
    virtual uint32_t hour() const;
    virtual uint32_t minute() const;
    virtual uint32_t second() const;
    virtual uint32_t nano() const;

    virtual std::unique_ptr<Temporal> withYear(int32_t y) const;
    virtual std::unique_ptr<Temporal> withMonth(uint32_t m) const;
    virtual std::unique_ptr<Temporal> withDayOfMonth(uint32_t d) const;
    virtual std::unique_ptr<Temporal> withHour(uint32_t h) const;
    virtual std::unique_ptr<Temporal> withMinute(uint32_t m) const;
    virtual std::unique_ptr<Temporal> withSecond(uint32_t s) const;
    virtual std::unique_ptr<Temporal> withNano(uint32_t n) const;

    virtual std::unique_ptr<Temporal> plusYears(int64_t years) const;
    virtual std::unique_ptr<Temporal> plusMonths(int64_t months) const;
    virtual std::unique_ptr<Temporal> plusWeeks(int64_t weeks) const;
    virtual std::unique_ptr<Temporal> plusDays(int64_t days) const;
    virtual std::unique_ptr<Temporal> plusHours(int64_t hours) const;
    virtual std::unique_ptr<Temporal> plusMinutes(int64_t minutes) const;
    virtual std::unique_ptr<Temporal> plusSeconds(int64_t seconds) const;
    virtual std::unique_ptr<Temporal> plusNanos(int64_t nanos) const;

    virtual std::unique_ptr<OffsetDateTime> atOffset(const ZoneOffset& offset) const;
    virtual std::unique_ptr<ZonedDateTime> atZone(const std::string& zoneId) const;

    virtual int compareTo(const LocalDateTime& other) const;

    virtual bool isAfter(const LocalDateTime& other) const;
    virtual bool isBefore(const LocalDateTime& other) const;
    virtual std::string toIsoString() const override;

    static bool isValidIsoString(const std::string& text);
    static std::unique_ptr<LocalDateTime> parseIsoString(const std::string& text);
};

} // namespace bas::time

#endif // BAS_TIME_LOCAL_DATE_TIME_HPP
