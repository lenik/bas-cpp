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
    virtual std::string toIsoString() const = 0;
};

} // namespace bas::time

#endif // BAS_TIME_OFFSET_DATE_TIME_HPP
