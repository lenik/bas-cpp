#ifndef OMNISHELL_CORE_REGISTRY_VALUE_IO_HPP
#define OMNISHELL_CORE_REGISTRY_VALUE_IO_HPP

#include "RegistryTypes.hpp"

#include <optional>
#include <string>

namespace os::reg {

std::string valueToString(const registry_value_t& v);
std::string variantToString(const variant_t& v);

/** Parse stored string into typed value; if ambiguous, returns string branch. */
variant_t parseValue(const std::string& s);

std::string optionalString(const variant_t& v, const std::string& def);
bool optionalBool(const variant_t& v, bool def);
int optionalInt(const variant_t& v, int def);
long optionalLong(const variant_t& v, long def);
float optionalFloat(const variant_t& v, float def);
double optionalDouble(const variant_t& v, double def);

/** Present and coercible to T, else nullopt (no value in variant => nullopt). */
std::optional<bool> tryBool(const variant_t& v);
std::optional<int> tryInt(const variant_t& v);
std::optional<long> tryLong(const variant_t& v);
std::optional<float> tryFloat(const variant_t& v);
std::optional<double> tryDouble(const variant_t& v);

std::optional<sys_time> trySysTime(const variant_t& v);
std::optional<local_time> tryLocalTime(const variant_t& v);
std::optional<zoned_time> tryZonedTime(const variant_t& v);
std::optional<year_month_day> tryYearMonthDay(const variant_t& v);
std::optional<time_of_day> tryTimeOfDay(const variant_t& v);

} // namespace os::reg

#endif
