#ifndef BAS_TIME_TEMPORAL_ACCESSOR_HPP
#define BAS_TIME_TEMPORAL_ACCESSOR_HPP

#include "TimeTypes.hpp"

#include <cstdint>
#include <optional>

namespace bas::time {

enum class TemporalField {
    // Proleptic year field, signed integer (can be negative for BCE-style proleptic timelines).
    Year,

    // Month-of-year field, 1-based: 1..12.
    Month,

    // Day-of-month field, 1-based: usually 1..28/29/30/31 depending on month/year.
    Day,

    // Day-of-year field, 1-based: 1..365/366 depending on leap year.
    DayOfYear,

    // Day-of-week field, 1-based with ISO-8601 semantics: 1=Monday ... 7=Sunday.
    DayOfWeek,

    // Hour-of-day field, 0-based: 0..23.
    Hour,

    // Minute-of-hour field, 0-based: 0..59.
    Minute,

    // Second-of-minute field, 0-based: 0..59 (leap-second handling is implementation-specific).
    Second,

    // Nano-of-second field, 0-based: 0..999,999,999.
    // This IS the fractional-second component field (unlike TemporalUnit::Nanos which is a
    // duration).
    Nano,

    // UTC offset total seconds field, signed, typically constrained to [-18h, +18h].
    OffsetSeconds,

    // Seconds from Unix epoch (1970-01-01T00:00:00Z), signed whole seconds.
    EpochSeconds,
};

class TemporalAccessor {
  public:
    virtual ~TemporalAccessor() = default;
    virtual bool isSupported(TemporalField field) const = 0;

    virtual std::optional<int32_t> get(TemporalField field) const;

    int32_t get(TemporalField field, int32_t defaultValue) const;

    virtual std::optional<int64_t> getLong(TemporalField field) const = 0;

    int64_t getLong(TemporalField field, int64_t defaultValue) const;

    virtual ValueRange range(TemporalField field) const;

    virtual std::optional<int64_t> query(const TemporalQuery& queryObj) const;
};

} // namespace bas::time

#endif // BAS_TIME_TEMPORAL_ACCESSOR_HPP
