#ifndef BAS_TIME_TIME_TYPES_HPP
#define BAS_TIME_TIME_TYPES_HPP

#include <cstdint>
#include <memory>
#include <optional>

namespace bas::time {

enum class DayOfWeek {
    Monday = 1,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday,
};

enum class Month {
    January = 1,
    February,
    March,
    April,
    May,
    June,
    July,
    August,
    September,
    October,
    November,
    December,
};

class ValueRange {
  public:
    ValueRange(int64_t minSmallest, int64_t minLargest, int64_t maxSmallest, int64_t maxLargest);

    static ValueRange of(int64_t min, int64_t max);

    int64_t minimum() const;
    int64_t largestMinimum() const;
    int64_t smallestMaximum() const;
    int64_t maximum() const;
    bool isValidValue(int64_t value) const;

  private:
    int64_t m_minSmallest;
    int64_t m_minLargest;
    int64_t m_maxSmallest;
    int64_t m_maxLargest;
};

class Temporal;
class TemporalAccessor;

class TemporalAmount {
  public:
    virtual ~TemporalAmount() = default;
    virtual std::unique_ptr<Temporal> addTo(const Temporal& temporal) const = 0;
    virtual std::unique_ptr<Temporal> subtractFrom(const Temporal& temporal) const = 0;
};

class TemporalAdjuster {
  public:
    virtual ~TemporalAdjuster() = default;
    virtual std::unique_ptr<Temporal> adjustInto(const Temporal& temporal) const = 0;
};

class TemporalQuery {
  public:
    virtual ~TemporalQuery() = default;
    virtual std::optional<int64_t> queryFrom(const TemporalAccessor& temporal) const = 0;
};

} // namespace bas::time

#endif // BAS_TIME_TIME_TYPES_HPP
