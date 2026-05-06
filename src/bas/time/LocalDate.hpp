#ifndef BAS_TIME_LOCAL_DATE_HPP
#define BAS_TIME_LOCAL_DATE_HPP

#include "Temporal.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace bas::time {

class LocalDateTime;
class LocalTime;
class OffsetDateTime;
class ZoneOffset;

class LocalDate : public Temporal {
public:
    ~LocalDate() override = default;

    virtual int32_t year() const = 0;
    virtual uint32_t month() const = 0;
    virtual uint32_t day() const = 0;
    virtual DayOfWeek dayOfWeek() const;
    virtual uint32_t dayOfYear() const;
    virtual bool isLeapYear() const;
    virtual uint32_t lengthOfMonth() const;
    virtual uint32_t lengthOfYear() const;

    virtual std::unique_ptr<Temporal> withYear(int32_t y) const;
    virtual std::unique_ptr<Temporal> withMonth(uint32_t m) const;
    virtual std::unique_ptr<Temporal> withDayOfMonth(uint32_t d) const;
    virtual std::unique_ptr<Temporal> plusYears(int64_t years) const;
    virtual std::unique_ptr<Temporal> plusMonths(int64_t months) const;
    virtual std::unique_ptr<Temporal> plusWeeks(int64_t weeks) const;
    virtual std::unique_ptr<Temporal> plusDays(int64_t days) const;
    virtual std::unique_ptr<Temporal> minusYears(int64_t years) const;
    virtual std::unique_ptr<Temporal> minusMonths(int64_t months) const;
    virtual std::unique_ptr<Temporal> minusWeeks(int64_t weeks) const;
    virtual std::unique_ptr<Temporal> minusDays(int64_t days) const;

    virtual std::unique_ptr<LocalDateTime> atStartOfDay() const;
    virtual std::unique_ptr<LocalDateTime> atTime(const LocalTime& time) const;
    virtual std::unique_ptr<OffsetDateTime> atTime(const LocalTime& time, const ZoneOffset& offset) const;

    virtual int compareTo(const LocalDate& other) const;

    virtual bool isAfter(const LocalDate& other) const;
    virtual bool isBefore(const LocalDate& other) const;
    virtual std::string toIsoString() const = 0;
};

} // namespace bas::time

#endif // BAS_TIME_LOCAL_DATE_HPP
