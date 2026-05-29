#ifndef BAS_TIME_ZONED_DATE_TIME_HPP
#define BAS_TIME_ZONED_DATE_TIME_HPP

#include "OffsetDateTime.hpp"

#include <memory>
#include <string>

namespace bas::time {

class Instant;
class LocalDateTime;

class ZonedDateTime : public Temporal {
  public:
    ~ZonedDateTime() override = default;

    virtual std::unique_ptr<OffsetDateTime> toOffsetDateTime() const = 0;
    virtual std::unique_ptr<LocalDateTime> toLocalDateTime() const;
    virtual std::unique_ptr<Instant> toInstant() const;
    virtual std::string zoneId() const = 0;
    virtual std::unique_ptr<Temporal> withZoneSameInstant(const std::string& newZoneId) const;
    virtual std::unique_ptr<Temporal> withZoneSameLocal(const std::string& newZoneId) const;
    virtual int compareTo(const ZonedDateTime& other) const;
    virtual std::string toIsoString() const override;

    static bool isValidIsoString(const std::string& text);
    static std::unique_ptr<ZonedDateTime> parseIsoString(const std::string& text);

    static std::unique_ptr<ZonedDateTime>
    fromTimePoint(const std::chrono::system_clock::time_point& timePoint);
};

} // namespace bas::time

#endif // BAS_TIME_ZONED_DATE_TIME_HPP
