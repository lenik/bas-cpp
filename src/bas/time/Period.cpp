#include "Period.hpp"

namespace bas::time {

Period::Period() : m_years(0), m_months(0), m_days(0) {}

Period::Period(int32_t years, int32_t months, int32_t days)
    : m_years(years), m_months(months), m_days(days) {}

Period Period::ofYears(int32_t years) { return Period(years, 0, 0); }

Period Period::ofMonths(int32_t months) { return Period(0, months, 0); }

Period Period::ofDays(int32_t days) { return Period(0, 0, days); }

Period Period::of(int32_t years, int32_t months, int32_t days) {
    return Period(years, months, days);
}

int32_t Period::years() const { return m_years; }

int32_t Period::months() const { return m_months; }

int32_t Period::days() const { return m_days; }

std::unique_ptr<Temporal> Period::addTo(const Temporal& temporal) const {
    auto t = temporal.plus(m_years, TemporalUnit::Years);
    t = t->plus(m_months, TemporalUnit::Months);
    return t->plus(m_days, TemporalUnit::Days);
}

std::unique_ptr<Temporal> Period::subtractFrom(const Temporal& temporal) const {
    auto t = temporal.minus(m_years, TemporalUnit::Years);
    t = t->minus(m_months, TemporalUnit::Months);
    return t->minus(m_days, TemporalUnit::Days);
}

} // namespace bas::time
