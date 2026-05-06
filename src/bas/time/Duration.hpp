#ifndef BAS_TIME_DURATION_HPP
#define BAS_TIME_DURATION_HPP

#include "Temporal.hpp"

#include <cstdint>
#include <memory>

namespace bas::time {

class Duration : public TemporalAmount {
public:
    Duration();
    Duration(int64_t seconds, int32_t nanos);

    static Duration ofSeconds(int64_t seconds);
    static Duration ofMillis(int64_t millis);
    static Duration ofNanos(int64_t nanos);

    int64_t seconds() const;
    int32_t nanos() const;

    std::unique_ptr<Temporal> addTo(const Temporal& temporal) const override;
    std::unique_ptr<Temporal> subtractFrom(const Temporal& temporal) const override;

private:
    int64_t m_seconds;
    int32_t m_nanos;
};

} // namespace bas::time

#endif // BAS_TIME_DURATION_HPP
