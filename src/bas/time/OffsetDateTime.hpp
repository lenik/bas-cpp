#ifndef BAS_TIME_OFFSET_DATE_TIME_HPP
#define BAS_TIME_OFFSET_DATE_TIME_HPP

#include "LocalDateTime.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace bas::time {

class Instant;
class ZonedDateTime;
class ZoneOffset;

class OffsetDateTime : public Temporal {
  public:
    ~OffsetDateTime() override = default;

    virtual std::unique_ptr<LocalDateTime> toLocalDateTime() const = 0;
    virtual int32_t offsetSeconds() const = 0;
    virtual std::unique_ptr<ZoneOffset> offset() const;
    virtual std::unique_ptr<Instant> toInstant() const;
    virtual std::unique_ptr<ZonedDateTime> toZonedDateTime() const;
    virtual std::unique_ptr<Temporal> withOffsetSameLocal(int32_t newOffsetSeconds) const;
    virtual std::unique_ptr<Temporal> withOffsetSameInstant(int32_t newOffsetSeconds) const;
    virtual int compareTo(const OffsetDateTime& other) const;
    virtual std::string toIsoString() const override;

    static bool isValidIsoString(const std::string& text);
    static std::unique_ptr<OffsetDateTime> parseIsoString(const std::string& text);

    static std::unique_ptr<OffsetDateTime>
    fromTimePoint(const std::chrono::system_clock::time_point& timePoint);
    static std::unique_ptr<OffsetDateTime>
    fromTimePoint(const std::chrono::system_clock::time_point& timePoint, int32_t offsetSeconds);
};

} // namespace bas::time

#endif // BAS_TIME_OFFSET_DATE_TIME_HPP
