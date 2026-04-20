#include "Instant.hpp"
#include "LocalDate.hpp"
#include "LocalDateTime.hpp"
#include "LocalTime.hpp"
#include "OffsetDateTime.hpp"
#include "OffsetTime.hpp"
#include "ZoneId.hpp"
#include "ZonedDateTime.hpp"

namespace bas::time {

std::unique_ptr<OffsetDateTime> Instant::atOffset(int32_t /*offsetSeconds*/) const {
    return nullptr;
}

std::unique_ptr<ZonedDateTime> Instant::atZone(const std::string& /*zoneId*/) const {
    return nullptr;
}

std::unique_ptr<LocalDateTime> LocalDate::atStartOfDay() const { return nullptr; }

std::unique_ptr<LocalDateTime> LocalDate::atTime(const LocalTime& /*time*/) const {
    return nullptr;
}

std::unique_ptr<OffsetDateTime> LocalDate::atTime(const LocalTime& /*time*/,
                                                  const ZoneOffset& /*offset*/) const {
    return nullptr;
}

std::unique_ptr<OffsetTime> LocalTime::atOffset(const ZoneOffset& /*offset*/) const {
    return nullptr;
}

std::unique_ptr<OffsetDateTime> LocalDateTime::atOffset(const ZoneOffset& /*offset*/) const {
    return nullptr;
}

std::unique_ptr<ZonedDateTime> LocalDateTime::atZone(const std::string& /*zoneId*/) const {
    return nullptr;
}

std::unique_ptr<ZoneOffset> OffsetTime::offset() const { return nullptr; }

std::unique_ptr<ZoneOffset> OffsetDateTime::offset() const { return nullptr; }

std::unique_ptr<Instant> OffsetDateTime::toInstant() const { return nullptr; }

std::unique_ptr<ZonedDateTime> OffsetDateTime::toZonedDateTime() const { return nullptr; }

std::unique_ptr<LocalDateTime> ZonedDateTime::toLocalDateTime() const {
    return toOffsetDateTime()->toLocalDateTime();
}

std::unique_ptr<Instant> ZonedDateTime::toInstant() const {
    return toOffsetDateTime()->toInstant();
}

std::unique_ptr<Temporal>
ZonedDateTime::withZoneSameInstant(const std::string& /*newZoneId*/) const {
    return nullptr;
}

std::unique_ptr<Temporal> ZonedDateTime::withZoneSameLocal(const std::string& /*newZoneId*/) const {
    return nullptr;
}

} // namespace bas::time
