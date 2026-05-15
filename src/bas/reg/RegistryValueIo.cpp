#include "RegistryValueIo.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>

namespace os::reg {

namespace {

std::string trim(std::string s) {
    auto notsp = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
    s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
    return s;
}

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

} // namespace

std::string valueToString(const registry_value_t& v) {
    return std::visit(
        [](auto&& x) -> std::string {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, bool>)
                return x ? "true" : "false";
            else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, long>)
                return std::to_string(x);
            else if constexpr (std::is_same_v<T, float>)
                return std::to_string(x);
            else if constexpr (std::is_same_v<T, double>)
                return std::to_string(x);
            else if constexpr (std::is_same_v<T, std::string>)
                return x;
            else if constexpr (std::is_same_v<T, sys_time>) {
                auto sec =
                    std::chrono::duration_cast<std::chrono::seconds>(x.time_since_epoch()).count();
                return std::string("sys:") + std::to_string(sec);
            } else if constexpr (std::is_same_v<T, local_time>) {
                auto sec =
                    std::chrono::duration_cast<std::chrono::seconds>(x.time_since_epoch()).count();
                return std::string("local:") + std::to_string(sec);
            } else if constexpr (std::is_same_v<T, zoned_time>) {
                auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                               x.get_sys_time().time_since_epoch())
                               .count();
                return std::string("zoned:") + x.get_time_zone()->name() + ":" +
                       std::to_string(sec);
            } else if constexpr (std::is_same_v<T, year_month_day>) {
                if (!x.ok())
                    return "ymd:0-0-0";
                return "ymd:" + std::to_string(static_cast<int>(x.year())) + "-" +
                       std::to_string(static_cast<unsigned>(x.month())) + "-" +
                       std::to_string(static_cast<unsigned>(x.day()));
            } else if constexpr (std::is_same_v<T, time_of_day>) {
                return "tod:" + std::to_string(x.hours().count()) + ":" +
                       std::to_string(x.minutes().count()) + ":" +
                       std::to_string(x.seconds().count());
            }
            return {};
        },
        v);
}

std::string variantToString(const variant_t& v) {
    if (!v.has_value())
        return "";
    return valueToString(*v);
}

variant_t parseValue(const std::string& raw) {
    std::string s = trim(raw);
    if (s.empty())
        return std::nullopt;

    if (iequals(s, "true"))
        return registry_value_t{true};
    if (iequals(s, "false"))
        return registry_value_t{false};

    if (s.rfind("ymd:", 0) == 0) {
        int y = 0, m = 0, d = 0;
        if (std::sscanf(s.c_str(), "ymd:%d-%d-%d", &y, &m, &d) >= 3) {
            year_month_day ymd{date::year{y}, date::month{static_cast<unsigned>(m)},
                               date::day{static_cast<unsigned>(d)}};
            if (ymd.ok())
                return registry_value_t{ymd};
        }
        return registry_value_t{s};
    }
    if (s.rfind("tod:", 0) == 0) {
        int H = 0, M = 0, Sec = 0;
        if (std::sscanf(s.c_str(), "tod:%d:%d:%d", &H, &M, &Sec) >= 3) {
            using namespace std::chrono;
            return registry_value_t{
                time_of_day{hours{H} + minutes{M} + seconds{static_cast<long long>(Sec)}}};
        }
        return registry_value_t{s};
    }
    if (s.rfind("sys:", 0) == 0) {
        long long sec = std::atoll(s.c_str() + 4);
        return registry_value_t{sys_time{std::chrono::seconds(sec)}};
    }
    if (s.rfind("zoned:", 0) == 0) {
        size_t last = s.rfind(':');
        if (last > 6 && last < s.size() - 1) {
            try {
                std::string zone = s.substr(6, last - 6);
                auto sec = std::chrono::seconds(std::atoll(s.c_str() + last + 1));
                zoned_time zt{zone, sys_time{sec}};
                return registry_value_t{zt};
            } catch (...) {
                return registry_value_t{s};
            }
        }
        return registry_value_t{s};
    }
    if (s.rfind("local:", 0) == 0) {
        long long sec = std::atoll(s.c_str() + 6);
        return registry_value_t{local_time{std::chrono::seconds(sec)}};
    }

    char* end = nullptr;
    long long li = std::strtoll(s.c_str(), &end, 10);
    if (end == s.c_str() + s.size()) {
        if (li >= INT_MIN && li <= INT_MAX)
            return registry_value_t{static_cast<int>(static_cast<int>(li))};
        return registry_value_t{static_cast<long>(li)};
    }

    end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    if (end == s.c_str() + s.size()) {
        float f = static_cast<float>(d);
        if (static_cast<double>(f) == d)
            return registry_value_t{f};
        return registry_value_t{d};
    }

    return registry_value_t{s};
}

std::string optionalString(const variant_t& v, const std::string& def) {
    if (!v.has_value())
        return def;
    if (auto* p = std::get_if<std::string>(&*v))
        return *p;
    return valueToString(*v);
}

bool optionalBool(const variant_t& v, bool def) {
    if (!v.has_value())
        return def;
    if (auto* p = std::get_if<bool>(&*v))
        return *p;
    if (auto* p = std::get_if<std::string>(&*v)) {
        if (iequals(*p, "true") || *p == "1")
            return true;
        if (iequals(*p, "false") || *p == "0")
            return false;
    }
    return def;
}

int optionalInt(const variant_t& v, int def) {
    if (!v.has_value())
        return def;
    if (auto* p = std::get_if<int>(&*v))
        return *p;
    if (auto* p = std::get_if<long>(&*v))
        return static_cast<int>(*p);
    if (auto* p = std::get_if<std::string>(&*v))
        return static_cast<int>(std::strtol(p->c_str(), nullptr, 10));
    return def;
}

long optionalLong(const variant_t& v, long def) {
    if (!v.has_value())
        return def;
    if (auto* p = std::get_if<long>(&*v))
        return *p;
    if (auto* p = std::get_if<int>(&*v))
        return *p;
    if (auto* p = std::get_if<std::string>(&*v))
        return std::strtol(p->c_str(), nullptr, 10);
    return def;
}

float optionalFloat(const variant_t& v, float def) {
    if (!v.has_value())
        return def;
    if (auto* p = std::get_if<float>(&*v))
        return *p;
    if (auto* p = std::get_if<double>(&*v))
        return static_cast<float>(*p);
    if (auto* p = std::get_if<std::string>(&*v))
        return std::strtof(p->c_str(), nullptr);
    return def;
}

double optionalDouble(const variant_t& v, double def) {
    if (!v.has_value())
        return def;
    if (auto* p = std::get_if<double>(&*v))
        return *p;
    if (auto* p = std::get_if<float>(&*v))
        return *p;
    if (auto* p = std::get_if<std::string>(&*v))
        return std::strtod(p->c_str(), nullptr);
    return def;
}

std::optional<bool> tryBool(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<bool>(&*v))
        return *p;
    if (auto* p = std::get_if<std::string>(&*v)) {
        if (iequals(*p, "true") || *p == "1")
            return true;
        if (iequals(*p, "false") || *p == "0")
            return false;
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<int> tryInt(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<int>(&*v))
        return *p;
    if (auto* p = std::get_if<long>(&*v))
        return static_cast<int>(*p);
    if (auto* p = std::get_if<std::string>(&*v)) {
        std::string s = trim(*p);
        if (s.empty())
            return std::nullopt;
        char* end = nullptr;
        long long x = std::strtoll(s.c_str(), &end, 10);
        if (end == s.c_str() || *end != '\0')
            return std::nullopt;
        if (x < INT_MIN || x > INT_MAX)
            return std::nullopt;
        return static_cast<int>(static_cast<int>(x));
    }
    return std::nullopt;
}

std::optional<long> tryLong(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<long>(&*v))
        return *p;
    if (auto* p = std::get_if<int>(&*v))
        return *p;
    if (auto* p = std::get_if<std::string>(&*v)) {
        std::string s = trim(*p);
        if (s.empty())
            return std::nullopt;
        char* end = nullptr;
        long long x = std::strtoll(s.c_str(), &end, 10);
        if (end == s.c_str() || *end != '\0')
            return std::nullopt;
        if (x < LONG_MIN || x > LONG_MAX)
            return std::nullopt;
        return static_cast<long>(x);
    }
    return std::nullopt;
}

std::optional<float> tryFloat(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<float>(&*v))
        return *p;
    if (auto* p = std::get_if<double>(&*v))
        return static_cast<float>(*p);
    if (auto* p = std::get_if<std::string>(&*v)) {
        std::string s = trim(*p);
        if (s.empty())
            return std::nullopt;
        char* end = nullptr;
        float x = std::strtof(s.c_str(), &end);
        if (end == s.c_str() || *end != '\0')
            return std::nullopt;
        return x;
    }
    return std::nullopt;
}

std::optional<double> tryDouble(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<double>(&*v))
        return *p;
    if (auto* p = std::get_if<float>(&*v))
        return *p;
    if (auto* p = std::get_if<std::string>(&*v)) {
        std::string s = trim(*p);
        if (s.empty())
            return std::nullopt;
        char* end = nullptr;
        double x = std::strtod(s.c_str(), &end);
        if (end == s.c_str() || *end != '\0')
            return std::nullopt;
        return x;
    }
    return std::nullopt;
}

std::optional<sys_time> trySysTime(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<sys_time>(&*v))
        return *p;
    return std::nullopt;
}

std::optional<local_time> tryLocalTime(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<local_time>(&*v))
        return *p;
    return std::nullopt;
}

std::optional<zoned_time> tryZonedTime(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<zoned_time>(&*v))
        return *p;
    return std::nullopt;
}

std::optional<year_month_day> tryYearMonthDay(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<year_month_day>(&*v))
        return *p;
    return std::nullopt;
}

std::optional<time_of_day> tryTimeOfDay(const variant_t& v) {
    if (!v.has_value())
        return std::nullopt;
    if (auto* p = std::get_if<time_of_day>(&*v))
        return *p;
    return std::nullopt;
}

} // namespace os::reg
