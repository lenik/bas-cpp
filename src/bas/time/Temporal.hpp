#ifndef BAS_TIME_TEMPORAL_HPP
#define BAS_TIME_TEMPORAL_HPP

#include "TemporalAccessor.hpp"
#include "TimeTypes.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace bas::time {

// Any 0-based / 1-based discussion belongs to TemporalField values (Hour/Month/etc), not
// TemporalUnit. Units are signed quantities used in arithmetic (plus/minus), not field slots.
enum class TemporalUnit {
    // Nanosecond duration unit (10^-9 second).
    //
    // IMPORTANT: this represents a complete elapsed-time amount in nanos (a delta), not just
    // "the fractional part of second". Example: plus(2'000'000'001, Nanos) means add 2 seconds
    // and 1 nanosecond in total.
    Nanos,

    // Whole-second duration unit. Used for epoch-based timeline math and coarse-grained arithmetic.
    Seconds,

    // Minute duration unit (60 seconds). Used for arithmetic only, no concept of 0/1 base here.
    Minutes,

    // Hour duration unit (60 minutes). Arithmetic quantity; field indexing is defined on
    // TemporalField::Hour (which is 0..23 in this API), not on this enum value.
    Hours,

    // Day duration/calendar step unit. For timeline types this is generally 24 hours; for local
    // calendar types the effect follows calendar rules (DST/zone behavior depends on concrete
    // type).
    Days,

    // Week unit (7 days). Convenience arithmetic unit; no base/index semantics.
    Weeks,

    // Calendar month unit (variable length). Not fixed seconds; resolved via calendar rules.
    Months,

    // Calendar year unit (variable length, leap-year aware). Calendar-based arithmetic step.
    Years,
};

class Temporal : public TemporalAccessor {
  public:
    ~Temporal() override = default;

    using TemporalAccessor::isSupported;
    virtual bool isSupported(TemporalUnit /*unit*/) const;
    
    virtual std::unique_ptr<Temporal> with(const TemporalAdjuster& adjuster) const;
    virtual std::unique_ptr<Temporal> with(TemporalField field, int64_t newValue) const = 0;
    virtual std::unique_ptr<Temporal> plus(const TemporalAmount& amount) const;
    virtual std::unique_ptr<Temporal> plus(int64_t amount, TemporalUnit unit) const = 0;

    std::unique_ptr<Temporal> minus(const TemporalAmount& amount) const;
    std::unique_ptr<Temporal> minus(int64_t amount, TemporalUnit unit) const;
    virtual std::optional<int64_t> until(const Temporal& /*endExclusive*/,
                                         TemporalUnit /*unit*/) const;

  public:
    int64_t epochNano(int64_t defaultValue = 0) const;
    int64_t epochMilli(int64_t defaultValue = 0) const;
    int64_t epochSecond(int64_t defaultValue = 0) const;
    int year(int defaultValue = 0) const;
    int month(int defaultValue = 0) const;
    int day(int defaultValue = 0) const;
    int dayOfWeek(int defaultValue = 0) const;
    int dayOfYear(int defaultValue = 0) const;
    int hour(int defaultValue = 0) const;
    int minute(int defaultValue = 0) const;
    int second(int defaultValue = 0) const;
    int nano(int defaultValue = 0) const;
    int offsetSeconds(int defaultValue = 0) const;

    virtual std::chrono::time_point<std::chrono::system_clock> toTimePoint() const;
    time_t toTime() const;
    struct tm toTm() const;

    std::filesystem::file_time_type toFileTime() const;

    virtual std::string toIsoString() const = 0;
};

} // namespace bas::time

#endif // BAS_TIME_TEMPORAL_HPP
