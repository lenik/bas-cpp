#ifndef BAS_REG_VARIANT_HPP
#define BAS_REG_VARIANT_HPP

#include <chrono>
#include <optional>
#include <string>
#include <variant>

#include <date/tz.h>

#include <boost/json.hpp>

namespace bas::reg {

enum regkey_kind {
    STRING_KEY = 0,
    INDEX_KEY,
};

using regkey_t = std::variant<std::string, size_t>;

std::string keyToString(const regkey_t& key);
regkey_t keyFromString(const std::string& s);

/** UTC instant (epoch seconds). */
using sys_time = date::sys_seconds;
/** Local-time-line instant (see Howard Hinnant date / local_t). */
using local_time = date::local_seconds;
/** Zoned instant (IANA zone + sys time); requires libdate-tz. */
using zoned_time = date::zoned_seconds;
using year_month_day = date::year_month_day;
using time_of_day = date::hh_mm_ss<std::chrono::seconds>;

enum value_kind {
    BOOL_VALUE = 0,
    INT_VALUE,
    LONG_VALUE,
    FLOAT_VALUE,
    DOUBLE_VALUE,
    STRING_VALUE,
    SYS_TIME_VALUE,
    LOCAL_TIME_VALUE,
    ZONED_TIME_VALUE,
    YEAR_MONTH_DAY_VALUE,
    TIME_OF_DAY_VALUE,
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
