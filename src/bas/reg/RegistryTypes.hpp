#ifndef OMNISHELL_CORE_REGISTRY_TYPES_HPP
#define OMNISHELL_CORE_REGISTRY_TYPES_HPP

#include <chrono>
#include <optional>
#include <string>
#include <variant>

#include <date/tz.h>

namespace os::reg {

/** UTC instant (epoch seconds). */
using sys_time = date::sys_seconds;
/** Local-time-line instant (see Howard Hinnant date / local_t). */
using local_time = date::local_seconds;
/** Zoned instant (IANA zone + sys time); requires libdate-tz. */
using zoned_time = date::zoned_seconds;
using year_month_day = date::year_month_day;
using time_of_day = date::hh_mm_ss<std::chrono::seconds>;

using variant_t = std::variant<bool, int, long, float, double, std::string, sys_time, local_time,
                                      zoned_time, year_month_day, time_of_day>;

using opt_variant_t = std::optional<variant_t>;

} // namespace os::reg

#endif
