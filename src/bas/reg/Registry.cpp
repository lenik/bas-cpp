#include "Registry.hpp"

namespace bas::reg {

IRegistry::~IRegistry() = default;

bool IRegistry::has(std::string_view path) const { return getOption(path).has_value(); }
bool IRegistry::remove(std::string_view path) {
    if (!has(path))
        return false;
    setOption(path, std::nullopt);
    return true;
}

reg::option_t IRegistry::get(std::string_view path, reg::option_t fallback) const {
    reg::option_t v = getOption(path);
    if (v.has_value())
        return v;
    return fallback;
}

reg::value_t IRegistry::get(std::string_view path, const reg::value_t& default_value) const {
    reg::option_t v = getOption(path);
    if (v.has_value())
        return *v;
    return default_value;
}

void IRegistry::set(std::string_view path, bool v) { setOption(path, v); }
void IRegistry::set(std::string_view path, int v) { setOption(path, v); }
void IRegistry::set(std::string_view path, long v) { setOption(path, v); }
void IRegistry::set(std::string_view path, float v) { setOption(path, v); }
void IRegistry::set(std::string_view path, double v) { setOption(path, v); }
void IRegistry::set(std::string_view path, const std::string& value) { setOption(path, value); }

std::optional<bool> IRegistry::getBool(std::string_view path, std::optional<bool> fallback) const {
    if (!has(path))
        return fallback;
    return reg::tryBool(getOption(path));
}

bool IRegistry::getBool(std::string_view path, bool default_value) const {
    return reg::optionalBool(getOption(path), default_value);
}

std::optional<int> IRegistry::getInt(std::string_view path, std::optional<int> fallback) const {
    if (!has(path))
        return fallback;
    return reg::tryInt(getOption(path));
}

int IRegistry::getInt(std::string_view path, int default_value) const {
    return reg::optionalInt(getOption(path), default_value);
}

std::optional<long> IRegistry::getLong(std::string_view path, std::optional<long> fallback) const {
    if (!has(path))
        return fallback;
    return reg::tryLong(getOption(path));
}

long IRegistry::getLong(std::string_view path, long default_value) const {
    return reg::optionalLong(getOption(path), default_value);
}

std::optional<float> IRegistry::getFloat(std::string_view path,
                                         std::optional<float> fallback) const {
    if (!has(path))
        return fallback;
    return reg::tryFloat(getOption(path));
}

float IRegistry::getFloat(std::string_view path, float default_value) const {
    return reg::optionalFloat(getOption(path), default_value);
}

std::optional<double> IRegistry::getDouble(std::string_view path,
                                           std::optional<double> fallback) const {
    if (!has(path))
        return fallback;
    return reg::tryDouble(getOption(path));
}

double IRegistry::getDouble(std::string_view path, double default_value) const {
    return reg::optionalDouble(getOption(path), default_value);
}

std::string IRegistry::getString(std::string_view path, const std::string& default_value) const {
    return reg::optionalString(getOption(path), default_value);
}

void IRegistry::set(std::string_view path, const reg::sys_time& v) { setOption(path, v); }
void IRegistry::set(std::string_view path, const reg::local_time& v) { setOption(path, v); }
void IRegistry::set(std::string_view path, const reg::zoned_time& v) { setOption(path, v); }
void IRegistry::set(std::string_view path, const reg::year_month_day& v) { setOption(path, v); }
void IRegistry::set(std::string_view path, const reg::time_of_day& v) { setOption(path, v); }

std::optional<reg::sys_time> IRegistry::getSysTime(std::string_view path,
                                                   std::optional<reg::sys_time> fallback) const {
    if (!has(path))
        return fallback;
    return reg::trySysTime(getOption(path));
}

reg::sys_time IRegistry::getSysTime(std::string_view path, reg::sys_time default_value) const {
    if (!has(path))
        return default_value;
    return reg::trySysTime(getOption(path)).value_or(default_value);
}

std::optional<reg::local_time>
IRegistry::getLocalTime(std::string_view path, std::optional<reg::local_time> fallback) const {
    if (!has(path))
        return fallback;
    return reg::tryLocalTime(getOption(path));
}

reg::local_time IRegistry::getLocalTime(std::string_view path,
                                        reg::local_time default_value) const {
    if (!has(path))
        return default_value;
    return reg::tryLocalTime(getOption(path)).value_or(default_value);
}

std::optional<reg::zoned_time>
IRegistry::getZonedTime(std::string_view path, std::optional<reg::zoned_time> fallback) const {
    if (!has(path))
        return fallback;
    return reg::tryZonedTime(getOption(path));
}

reg::zoned_time IRegistry::getZonedTime(std::string_view path,
                                        const reg::zoned_time& default_value) const {
    if (!has(path))
        return default_value;
    auto t = reg::tryZonedTime(getOption(path));
    return t.value_or(default_value);
}

std::optional<reg::year_month_day>
IRegistry::getYearMonthDay(std::string_view path,
                           std::optional<reg::year_month_day> fallback) const {
    if (!has(path))
        return fallback;
    return reg::tryYearMonthDay(getOption(path));
}

reg::year_month_day IRegistry::getYearMonthDay(std::string_view path,
                                               reg::year_month_day default_value) const {
    if (!has(path))
        return default_value;
    return reg::tryYearMonthDay(getOption(path)).value_or(default_value);
}

std::optional<reg::time_of_day>
IRegistry::getTimeOfDay(std::string_view path, std::optional<reg::time_of_day> fallback) const {
    if (!has(path))
        return fallback;
    return reg::tryTimeOfDay(getOption(path));
}

reg::time_of_day IRegistry::getTimeOfDay(std::string_view path,
                                         reg::time_of_day default_value) const {
    if (!has(path))
        return default_value;
    return reg::tryTimeOfDay(getOption(path)).value_or(default_value);
}

IRegistry::watch_handle IRegistry::watch(std::string_view prefix, changed_slot slot) {
    return m_changed.connect([prefix, cb = std::move(slot)](const std::string& path,
                                                            const reg::option_t& new_value,
                                                            const reg::option_t& old_value) {
        if (!cb)
            return;
        const bool match =
            prefix.empty() || path == prefix || path.rfind(std::string(prefix) + ".", 0) == 0;
        if (match)
            cb(path, new_value, old_value);
    });
}

void IRegistry::unwatch(watch_handle h) { h.disconnect(); }

void IRegistry::emitChanged(const std::string& path, const reg::option_t& new_value,
                            const reg::option_t& old_value) {
    m_changed(path, new_value, old_value);
}

} // namespace bas::reg
