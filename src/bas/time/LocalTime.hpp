#ifndef BAS_TIME_LOCAL_TIME_HPP
#define BAS_TIME_LOCAL_TIME_HPP

#include "Temporal.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace bas::time {

class OffsetTime;
class ZoneOffset;

class LocalTime : public Temporal {
public:
    ~LocalTime() override = default;

    virtual uint32_t hour() const = 0;
    virtual uint32_t minute() const = 0;
    virtual uint32_t second() const = 0;
    virtual uint32_t nano() const = 0;

    virtual std::unique_ptr<Temporal> withHour(uint32_t h) const;
    virtual std::unique_ptr<Temporal> withMinute(uint32_t m) const;
    virtual std::unique_ptr<Temporal> withSecond(uint32_t s) const;
    virtual std::unique_ptr<Temporal> withNano(uint32_t n) const;

    virtual std::unique_ptr<Temporal> plusHours(int64_t hours) const;
    virtual std::unique_ptr<Temporal> plusMinutes(int64_t minutes) const;
    virtual std::unique_ptr<Temporal> plusSeconds(int64_t seconds) const;
    virtual std::unique_ptr<Temporal> plusNanos(int64_t nanos) const;
    virtual std::unique_ptr<Temporal> minusHours(int64_t hours) const;
    virtual std::unique_ptr<Temporal> minusMinutes(int64_t minutes) const;
    virtual std::unique_ptr<Temporal> minusSeconds(int64_t seconds) const;
    virtual std::unique_ptr<Temporal> minusNanos(int64_t nanos) const;

    virtual int compareTo(const LocalTime& other) const;

    virtual bool isAfter(const LocalTime& other) const;
    virtual bool isBefore(const LocalTime& other) const;
    virtual int64_t toSecondOfDay() const;
    virtual int64_t toNanoOfDay() const;
    virtual std::unique_ptr<OffsetTime> atOffset(const ZoneOffset& offset) const;
    virtual std::string toIsoString() const = 0;
};

} // namespace bas::time

#endif // BAS_TIME_LOCAL_TIME_HPP
