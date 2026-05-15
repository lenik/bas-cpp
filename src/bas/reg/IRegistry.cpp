#include "IRegistry.hpp"

#include "RegistryValueIo.hpp"

namespace os {

IRegistry::~IRegistry() = default;

reg::variant_t IRegistry::get(const std::string& key, reg::variant_t fallback) const {
    reg::variant_t v = getVariant(key);
    if (v.has_value())
        return v;
    return fallback;
}

reg::registry_value_t IRegistry::get(const std::string& key,
                                   const reg::registry_value_t& default_value) const {
    reg::variant_t v = getVariant(key);
    if (v.has_value())
        return *v;
    return default_value;
}

void IRegistry::set(const std::string& key, const std::string& value) {
    setVariant(key, reg::registry_value_t{value});
}

std::string IRegistry::getString(const std::string& key, const std::string& default_value) const {
    return reg::optionalString(getVariant(key), default_value);
}

void IRegistry::set(const std::string& key, bool v) {
    setVariant(key, reg::registry_value_t{v});
}

void IRegistry::set(const std::string& key, int v) {
    setVariant(key, reg::registry_value_t{v});
}

void IRegistry::set(const std::string& key, long v) {
    setVariant(key, reg::registry_value_t{v});
}

void IRegistry::set(const std::string& key, float v) {
    setVariant(key, reg::registry_value_t{v});
}

void IRegistry::set(const std::string& key, double v) {
    setVariant(key, reg::registry_value_t{v});
}

std::optional<bool> IRegistry::getBool(const std::string& key, std::optional<bool> fallback) const {
    if (!has(key))
        return fallback;
    return reg::tryBool(getVariant(key));
}

bool IRegistry::getBool(const std::string& key, bool default_value) const {
    return reg::optionalBool(getVariant(key), default_value);
}

std::optional<int> IRegistry::getInt(const std::string& key, std::optional<int> fallback) const {
    if (!has(key))
        return fallback;
    return reg::tryInt(getVariant(key));
}

int IRegistry::getInt(const std::string& key, int default_value) const {
    return reg::optionalInt(getVariant(key), default_value);
}

std::optional<long> IRegistry::getLong(const std::string& key, std::optional<long> fallback) const {
    if (!has(key))
        return fallback;
    return reg::tryLong(getVariant(key));
}

long IRegistry::getLong(const std::string& key, long default_value) const {
    return reg::optionalLong(getVariant(key), default_value);
}

std::optional<float> IRegistry::getFloat(const std::string& key, std::optional<float> fallback) const {
    if (!has(key))
        return fallback;
    return reg::tryFloat(getVariant(key));
}

float IRegistry::getFloat(const std::string& key, float default_value) const {
    return reg::optionalFloat(getVariant(key), default_value);
}

std::optional<double> IRegistry::getDouble(const std::string& key, std::optional<double> fallback) const {
    if (!has(key))
        return fallback;
    return reg::tryDouble(getVariant(key));
}

double IRegistry::getDouble(const std::string& key, double default_value) const {
    return reg::optionalDouble(getVariant(key), default_value);
}

void IRegistry::set(const std::string& key, const reg::sys_time& v) {
    setVariant(key, reg::registry_value_t{v});
}

void IRegistry::set(const std::string& key, const reg::local_time& v) {
    setVariant(key, reg::registry_value_t{v});
}

void IRegistry::set(const std::string& key, const reg::zoned_time& v) {
    setVariant(key, reg::registry_value_t{v});
}

void IRegistry::set(const std::string& key, const reg::year_month_day& v) {
    setVariant(key, reg::registry_value_t{v});
}

void IRegistry::set(const std::string& key, const reg::time_of_day& v) {
    setVariant(key, reg::registry_value_t{v});
}

std::optional<reg::sys_time> IRegistry::getSysTime(const std::string& key,
                                                    std::optional<reg::sys_time> fallback) const {
    if (!has(key))
        return fallback;
    return reg::trySysTime(getVariant(key));
}

reg::sys_time IRegistry::getSysTime(const std::string& key, reg::sys_time default_value) const {
    if (!has(key))
        return default_value;
    return reg::trySysTime(getVariant(key)).value_or(default_value);
}

std::optional<reg::local_time> IRegistry::getLocalTime(const std::string& key,
                                                       std::optional<reg::local_time> fallback) const {
    if (!has(key))
        return fallback;
    return reg::tryLocalTime(getVariant(key));
}

reg::local_time IRegistry::getLocalTime(const std::string& key, reg::local_time default_value) const {
    if (!has(key))
        return default_value;
    return reg::tryLocalTime(getVariant(key)).value_or(default_value);
}

std::optional<reg::zoned_time> IRegistry::getZonedTime(const std::string& key,
                                                       std::optional<reg::zoned_time> fallback) const {
    if (!has(key))
        return fallback;
    return reg::tryZonedTime(getVariant(key));
}

reg::zoned_time IRegistry::getZonedTime(const std::string& key,
                                        const reg::zoned_time& default_value) const {
    if (!has(key))
        return default_value;
    auto t = reg::tryZonedTime(getVariant(key));
    return t.value_or(default_value);
}

std::optional<reg::year_month_day> IRegistry::getYearMonthDay(
    const std::string& key, std::optional<reg::year_month_day> fallback) const {
    if (!has(key))
        return fallback;
    return reg::tryYearMonthDay(getVariant(key));
}

reg::year_month_day IRegistry::getYearMonthDay(const std::string& key,
                                               reg::year_month_day default_value) const {
    if (!has(key))
        return default_value;
    return reg::tryYearMonthDay(getVariant(key)).value_or(default_value);
}

std::optional<reg::time_of_day> IRegistry::getTimeOfDay(const std::string& key,
                                                        std::optional<reg::time_of_day> fallback) const {
    if (!has(key))
        return fallback;
    return reg::tryTimeOfDay(getVariant(key));
}

reg::time_of_day IRegistry::getTimeOfDay(const std::string& key,
                                         reg::time_of_day default_value) const {
    if (!has(key))
        return default_value;
    return reg::tryTimeOfDay(getVariant(key)).value_or(default_value);
}

IRegistry::watch_handle IRegistry::watch(const std::string& prefix, changed_slot slot) {
    return m_changed.connect(
        [prefix, cb = std::move(slot)](const std::string& key, const reg::variant_t& new_value,
                                       const reg::variant_t& old_value) {
            if (!cb)
                return;
            const bool match =
                prefix.empty() || key == prefix || key.rfind(prefix + ".", 0) == 0;
            if (match)
                cb(key, new_value, old_value);
        });
}

void IRegistry::unwatch(watch_handle h) {
    h.disconnect();
}

void IRegistry::emitChanged(const std::string& key, const reg::variant_t& new_value,
                            const reg::variant_t& old_value) {
    m_changed(key, new_value, old_value);
}

} // namespace os
