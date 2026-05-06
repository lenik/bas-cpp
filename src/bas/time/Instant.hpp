#ifndef BAS_TIME_INSTANT_HPP
#define BAS_TIME_INSTANT_HPP

#include "Temporal.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace bas::time {

class OffsetDateTime;
class ZonedDateTime;

class Instant : public Temporal {
  public:
    ~Instant() override = default;

    virtual int32_t nano() const = 0;
    virtual int64_t epochSecond() const = 0;
    virtual int64_t toEpochMilli() const;

    virtual std::unique_ptr<Temporal> truncatedTo(TemporalUnit /*unit*/) const;
    virtual std::unique_ptr<Temporal> plusSeconds(int64_t seconds) const;
    virtual std::unique_ptr<Temporal> plusMillis(int64_t millis) const;
    virtual std::unique_ptr<Temporal> plusNanos(int64_t nanos) const;
    virtual std::unique_ptr<Temporal> minusSeconds(int64_t seconds) const;
    virtual std::unique_ptr<Temporal> minusMillis(int64_t millis) const;
    virtual std::unique_ptr<Temporal> minusNanos(int64_t nanos) const;

    virtual std::unique_ptr<OffsetDateTime> atOffset(int32_t offsetSeconds) const;
    virtual std::unique_ptr<ZonedDateTime> atZone(const std::string& zoneId) const;

    virtual int compareTo(const Instant& other) const;

    virtual bool isAfter(const Instant& other) const;
    virtual bool isBefore(const Instant& other) const;
    virtual std::string toIsoString() const = 0;
};

} // namespace bas::time

#endif // BAS_TIME_INSTANT_HPP
