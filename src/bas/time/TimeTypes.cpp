#include "TimeTypes.hpp"

namespace bas::time {

ValueRange::ValueRange(int64_t minSmallest, int64_t minLargest, int64_t maxSmallest,
                       int64_t maxLargest)
    : m_minSmallest(minSmallest), m_minLargest(minLargest), m_maxSmallest(maxSmallest),
      m_maxLargest(maxLargest) {}

ValueRange ValueRange::of(int64_t min, int64_t max) { return ValueRange(min, min, max, max); }

int64_t ValueRange::minimum() const { return m_minSmallest; }

int64_t ValueRange::largestMinimum() const { return m_minLargest; }

int64_t ValueRange::smallestMaximum() const { return m_maxSmallest; }

int64_t ValueRange::maximum() const { return m_maxLargest; }

bool ValueRange::isValidValue(int64_t value) const {
    return value >= m_minSmallest && value <= m_maxLargest;
}

} // namespace bas::time
