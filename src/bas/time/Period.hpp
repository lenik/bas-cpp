#ifndef BAS_TIME_PERIOD_HPP
#define BAS_TIME_PERIOD_HPP

#include <cstdint>
#include <memory>

#include "Temporal.hpp"

namespace bas::time {

class Period : public TemporalAmount {
public:
    Period();
    Period(int32_t years, int32_t months, int32_t days);

    static Period ofYears(int32_t years);
    static Period ofMonths(int32_t months);
    static Period ofDays(int32_t days);
    static Period of(int32_t years, int32_t months, int32_t days);

    int32_t years() const;
    int32_t months() const;
    int32_t days() const;

    std::unique_ptr<Temporal> addTo(const Temporal& temporal) const override;
    std::unique_ptr<Temporal> subtractFrom(const Temporal& temporal) const override;

private:
    int32_t m_years;
    int32_t m_months;
    int32_t m_days;
};

} // namespace bas::time

#endif // BAS_TIME_PERIOD_HPP
