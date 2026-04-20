#include "TemporalAccessor.hpp"

namespace bas::time {

std::optional<int32_t> TemporalAccessor::get(TemporalField field) const {
    const auto value = getLong(field);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return static_cast<int32_t>(*value);
}

int32_t TemporalAccessor::get(TemporalField field, int32_t defaultValue) const {
    const auto value = get(field);
    if (!value.has_value()) {
        return defaultValue;
    }
    return *value;
}

int64_t TemporalAccessor::getLong(TemporalField field, int64_t defaultValue) const {
    const auto value = getLong(field);
    if (!value.has_value()) {
        return defaultValue;
    }
    return value.value();
}

ValueRange TemporalAccessor::range(TemporalField field) const {
    if (field == TemporalField::Month) {
        return ValueRange::of(1, 12);
    }
    if (field == TemporalField::Day) {
        return ValueRange::of(1, 31);
    }
    if (field == TemporalField::DayOfYear) {
        // Per-date effective range is 1..365 or 1..366; keep generic max here.
        return ValueRange::of(1, 366);
    }
    if (field == TemporalField::DayOfWeek) {
        return ValueRange::of(1, 7);
    }
    if (field == TemporalField::Hour) {
        return ValueRange::of(0, 23);
    }
    if (field == TemporalField::Minute || field == TemporalField::Second) {
        return ValueRange::of(0, 59);
    }
    if (field == TemporalField::Nano) {
        return ValueRange::of(0, 999999999);
    }
    if (field == TemporalField::OffsetSeconds) {
        return ValueRange::of(-18 * 3600, 18 * 3600);
    }
    return ValueRange::of(-999999999, 999999999);
}

std::optional<int64_t> TemporalAccessor::query(const TemporalQuery& queryObj) const {
    return queryObj.queryFrom(*this);
}

} // namespace bas::time
