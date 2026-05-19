#ifndef BAS_REG_REGISTRY_HPP
#define BAS_REG_REGISTRY_HPP

#include "variant.hpp"

#include <boost/signals2/signal.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace bas::reg {

/**
 * Abstract registry: typed values, tree listing, persistence hooks, change notifications.
 */
class IRegistry {
  public:
    using changed_slot = std::function<void(const std::string& path, const reg::option_t& new_value,
                                            const reg::option_t& old_value)>;
    using watch_handle = boost::signals2::connection;

    virtual ~IRegistry();

    virtual std::vector<std::string> listDir(std::string_view path) const = 0;
    virtual std::vector<std::string> listDomain(std::string_view path) const = 0;

    /** List all keys in the registry. (exactly union(listDir, listDomain)) */
    virtual std::vector<std::string> list(std::string_view path) const = 0;

    /** Remove key and all descendants (prefix tree). @return true if anything removed. */
    virtual bool delTree(std::string_view path) = 0;

    virtual reg::option_t getOption(std::string_view path) const = 0;
    virtual void setOption(std::string_view path, reg::option_t value) = 0;

    /** Missing key or only fallback → return fallback; else stored value. */
    reg::option_t get(std::string_view path, reg::option_t fallback = std::nullopt) const;

    /** Missing key → return default_value; else stored value. */
    reg::value_t get(std::string_view path, const reg::value_t& default_value) const;

    bool has(std::string_view path) const;
    /** Remove leaf key. @return true if existed. */
    bool remove(std::string_view path);

    void set(std::string_view key, bool v);
    void set(std::string_view key, int v);
    void set(std::string_view key, long v);
    void set(std::string_view key, float v);
    void set(std::string_view key, double v);
    void set(std::string_view key, const std::string& value);

    std::optional<bool> getBool(std::string_view key,
                                std::optional<bool> fallback = std::nullopt) const;
    bool getBool(std::string_view key, bool default_value) const;
    std::optional<int> getInt(std::string_view key,
                              std::optional<int> fallback = std::nullopt) const;
    int getInt(std::string_view key, int default_value) const;
    std::optional<long> getLong(std::string_view key,
                                std::optional<long> fallback = std::nullopt) const;
    long getLong(std::string_view key, long default_value) const;
    std::optional<float> getFloat(std::string_view key,
                                  std::optional<float> fallback = std::nullopt) const;
    float getFloat(std::string_view key, float default_value) const;
    std::optional<double> getDouble(std::string_view key,
                                    std::optional<double> fallback = std::nullopt) const;
    double getDouble(std::string_view key, double default_value) const;
    std::optional<std::string> getString(std::string_view key,
                                         std::optional<std::string> fallback = std::nullopt) const;
    std::string getString(std::string_view key, const std::string& default_value) const;

    void set(std::string_view key, const reg::sys_time& v);
    void set(std::string_view key, const reg::local_time& v);
    void set(std::string_view key, const reg::zoned_time& v);
    void set(std::string_view key, const reg::year_month_day& v);
    void set(std::string_view key, const reg::time_of_day& v);

    std::optional<reg::sys_time>
    getSysTime(std::string_view key, std::optional<reg::sys_time> fallback = std::nullopt) const;
    reg::sys_time getSysTime(std::string_view key, reg::sys_time default_value) const;
    std::optional<reg::local_time>
    getLocalTime(std::string_view key,
                 std::optional<reg::local_time> fallback = std::nullopt) const;
    reg::local_time getLocalTime(std::string_view key, reg::local_time default_value) const;
    std::optional<reg::zoned_time>
    getZonedTime(std::string_view key,
                 std::optional<reg::zoned_time> fallback = std::nullopt) const;
    reg::zoned_time getZonedTime(std::string_view key, const reg::zoned_time& default_value) const;
    std::optional<reg::year_month_day>
    getYearMonthDay(std::string_view key,
                    std::optional<reg::year_month_day> fallback = std::nullopt) const;
    reg::year_month_day getYearMonthDay(std::string_view key,
                                        reg::year_month_day default_value) const;
    std::optional<reg::time_of_day>
    getTimeOfDay(std::string_view key,
                 std::optional<reg::time_of_day> fallback = std::nullopt) const;
    reg::time_of_day getTimeOfDay(std::string_view key, reg::time_of_day default_value) const;

    /** Subscribe; slot runs when key matches prefix (empty = all keys). */
    watch_handle watch(std::string_view prefix, changed_slot slot);
    void unwatch(watch_handle h);

  protected:
    void emitChanged(const std::string& key, const reg::option_t& new_value,
                     const reg::option_t& old_value);

  private:
    boost::signals2::signal<void(const std::string&, const reg::option_t&, const reg::option_t&)>
        m_changed;
};

} // namespace bas::reg

#endif // BAS_REG_REGISTRY_HPP
