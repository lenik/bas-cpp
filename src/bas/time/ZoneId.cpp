#include "ZoneId.hpp"

namespace bas::time {

ZoneId::ZoneId(std::string value) : m_value(std::move(value)) {}

std::string ZoneId::id() const { return m_value; }

ZoneOffset::ZoneOffset(int32_t seconds) : m_seconds(seconds) {}

int32_t ZoneOffset::totalSeconds() const { return m_seconds; }

std::string ZoneOffset::id() const {
    const auto sign = m_seconds < 0 ? '-' : '+';
    const auto absOff = m_seconds < 0 ? -m_seconds : m_seconds;
    const auto hh = absOff / 3600;
    const auto mm = (absOff % 3600) / 60;
    return std::string(1, sign) + (hh < 10 ? "0" : "") + std::to_string(hh) + ":" +
           (mm < 10 ? "0" : "") + std::to_string(mm);
}

} // namespace bas::time
