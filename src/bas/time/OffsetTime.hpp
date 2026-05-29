#ifndef BAS_TIME_OFFSET_TIME_HPP
#define BAS_TIME_OFFSET_TIME_HPP

#include "LocalTime.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace bas::time {

class ZoneOffset;

class OffsetTime : public Temporal {
public:
    ~OffsetTime() override = default;

    virtual std::unique_ptr<LocalTime> toLocalTime() const = 0;
    virtual int32_t offsetSeconds() const = 0;
    virtual std::unique_ptr<ZoneOffset> offset() const;
    virtual std::unique_ptr<Temporal> withOffsetSameLocal(int32_t newOffsetSeconds) const;
    virtual std::unique_ptr<Temporal> withOffsetSameInstant(int32_t newOffsetSeconds) const;
    virtual int compareTo(const OffsetTime& other) const;
    virtual std::string toIsoString() const override;

    static bool isValidIsoString(const std::string& text);
    static std::unique_ptr<OffsetTime> parseIsoString(const std::string& text);
};

} // namespace bas::time

#endif // BAS_TIME_OFFSET_TIME_HPP
