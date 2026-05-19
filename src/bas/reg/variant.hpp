#ifndef BAS_REG_VARIANT_HPP
#define BAS_REG_VARIANT_HPP

#include <chrono>
#include <optional>
#include <string>
#include <variant>

#include <date/tz.h>

#include <boost/json.hpp>

namespace bas::reg {

/** UTC instant (epoch seconds). */
using sys_time = date::sys_seconds;
/** Local-time-line instant (see Howard Hinnant date / local_t). */
using local_time = date::local_seconds;
/** Zoned instant (IANA zone + sys time); requires libdate-tz. */
using zoned_time = date::zoned_seconds;
using year_month_day = date::year_month_day;
using time_of_day = date::hh_mm_ss<std::chrono::seconds>;

enum value_kind {
    bool_kind = 0,
    int_kind,
    long_kind,
    float_kind,
    double_kind,
    string_kind,
    sys_time_kind,
    local_time_kind,
    zoned_time_kind,
    year_month_day_kind,
    time_of_day_kind,
};

using value_t = std::variant<bool, int, long, float, double, std::string, sys_time, local_time,
                             zoned_time, year_month_day, time_of_day>;

using option_t = std::optional<value_t>;

option_t jsonToOption(const boost::json::value& v);
option_t jsonToOption(const std::optional<boost::json::value> v);
boost::json::value optionToJson(const option_t& v);

std::string valueToString(const value_t& v);
std::string optionToString(const option_t& v);

/** Parse stored string into typed value; if ambiguous, returns string branch. */
option_t parseOption(const std::string& s);

bool optionalBool(const option_t& v, bool def);
int optionalInt(const option_t& v, int def);
long optionalLong(const option_t& v, long def);
float optionalFloat(const option_t& v, float def);
double optionalDouble(const option_t& v, double def);
std::string optionalString(const option_t& v, const std::string& def);

/** Present and coercible to T, else nullopt (no value in option => nullopt). */
std::optional<bool> tryBool(const option_t& v);
std::optional<int> tryInt(const option_t& v);
std::optional<long> tryLong(const option_t& v);
std::optional<float> tryFloat(const option_t& v);
std::optional<double> tryDouble(const option_t& v);

std::optional<sys_time> trySysTime(const option_t& v);
std::optional<local_time> tryLocalTime(const option_t& v);
std::optional<zoned_time> tryZonedTime(const option_t& v);
std::optional<year_month_day> tryYearMonthDay(const option_t& v);
std::optional<time_of_day> tryTimeOfDay(const option_t& v);

} // namespace bas::reg

#endif // BAS_REG_VARIANT_HPP
