#ifndef OMNISHELL_CORE_IREGISTRY_HPP
#define OMNISHELL_CORE_IREGISTRY_HPP

#include "RegistryTypes.hpp"

#include "bas/fmt/FileForm.hpp"

#include <boost/signals2/signal.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace os {

/**
 * Abstract registry: typed values, tree listing, persistence hooks, change notifications.
 */
class IRegistry {
  public:
    using changed_slot =
        std::function<void(const std::string& key, const reg::opt_variant_t& new_value,
                           const reg::opt_variant_t& old_value)>;
    using watch_handle = boost::signals2::connection;

    virtual ~IRegistry();

    /** Children of node_key ("" = roots). full_key: "a.b.c" vs "c". */
    virtual std::vector<std::string> list(const std::string& node_key, bool full_key) const = 0;

    /** Remove key and all descendants (prefix tree). @return true if anything removed. */
    virtual bool delTree(const std::string& path) = 0;

    virtual reg::opt_variant_t getVariant(const std::string& key) const = 0;
    virtual void setVariant(const std::string& key, reg::opt_variant_t value) = 0;

    virtual bool hasValue(const std::string& path) const = 0;
    /** Remove leaf key. @return true if existed. */
    virtual bool removeValue(const std::string& path) = 0;

    // following: get/set() means getValue/setValue()

    /** Missing key or only fallback → return fallback; else stored value. */
    virtual reg::opt_variant_t get(const std::string& path,
                                   reg::opt_variant_t fallback = std::nullopt) const;

    /** Missing key → return default_value; else stored value. */
    virtual reg::variant_t get(const std::string& path, const reg::variant_t& default_value) const;

    void set(const std::string& key, bool v);
    void set(const std::string& key, int v);
    void set(const std::string& key, long v);
    void set(const std::string& key, float v);
    void set(const std::string& key, double v);
    void set(const std::string& key, const std::string& value);

    std::optional<bool> getBool(const std::string& key,
                                std::optional<bool> fallback = std::nullopt) const;
    bool getBool(const std::string& key, bool default_value) const;
    std::optional<int> getInt(const std::string& key,
                              std::optional<int> fallback = std::nullopt) const;
    int getInt(const std::string& key, int default_value) const;
    std::optional<long> getLong(const std::string& key,
                                std::optional<long> fallback = std::nullopt) const;
    long getLong(const std::string& key, long default_value) const;
    std::optional<float> getFloat(const std::string& key,
                                  std::optional<float> fallback = std::nullopt) const;
    float getFloat(const std::string& key, float default_value) const;
    std::optional<double> getDouble(const std::string& key,
                                    std::optional<double> fallback = std::nullopt) const;
    double getDouble(const std::string& key, double default_value) const;
    std::optional<std::string> getString(const std::string& key,
                                         std::optional<std::string> fallback = std::nullopt) const;
    std::string getString(const std::string& key, const std::string& default_value) const;

    void set(const std::string& key, const reg::sys_time& v);
    void set(const std::string& key, const reg::local_time& v);
    void set(const std::string& key, const reg::zoned_time& v);
    void set(const std::string& key, const reg::year_month_day& v);
    void set(const std::string& key, const reg::time_of_day& v);

    std::optional<reg::sys_time>
    getSysTime(const std::string& key, std::optional<reg::sys_time> fallback = std::nullopt) const;
    reg::sys_time getSysTime(const std::string& key, reg::sys_time default_value) const;
    std::optional<reg::local_time>
    getLocalTime(const std::string& key,
                 std::optional<reg::local_time> fallback = std::nullopt) const;
    reg::local_time getLocalTime(const std::string& key, reg::local_time default_value) const;
    std::optional<reg::zoned_time>
    getZonedTime(const std::string& key,
                 std::optional<reg::zoned_time> fallback = std::nullopt) const;
    reg::zoned_time getZonedTime(const std::string& key,
                                 const reg::zoned_time& default_value) const;
    std::optional<reg::year_month_day>
    getYearMonthDay(const std::string& key,
                    std::optional<reg::year_month_day> fallback = std::nullopt) const;
    reg::year_month_day getYearMonthDay(const std::string& key,
                                        reg::year_month_day default_value) const;
    std::optional<reg::time_of_day>
    getTimeOfDay(const std::string& key,
                 std::optional<reg::time_of_day> fallback = std::nullopt) const;
    reg::time_of_day getTimeOfDay(const std::string& key, reg::time_of_day default_value) const;

    /** Subscribe; slot runs when key matches prefix (empty = all keys). */
    watch_handle watch(const std::string& prefix, changed_slot slot);
    void unwatch(watch_handle h);

  protected:
    void emitChanged(const std::string& key, const reg::opt_variant_t& new_value,
                     const reg::opt_variant_t& old_value);

  private:
    boost::signals2::signal<void(const std::string&, const reg::opt_variant_t&,
                                 const reg::opt_variant_t&)>
        m_changed;
};

} // namespace os

#endif
