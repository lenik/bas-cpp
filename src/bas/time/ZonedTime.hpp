#ifndef BAS_TIME_ZONED_TIME_HPP
#define BAS_TIME_ZONED_TIME_HPP

#include <string>
#include <memory>
#include <optional>

#include "OffsetTime.hpp"

namespace bas::time {

class ZonedTime : public Temporal {
public:
    ~ZonedTime() override = default;

    virtual std::unique_ptr<OffsetTime> toOffsetTime() const = 0;
    virtual std::string zoneId() const = 0;
    virtual int compareTo(const ZonedTime& other) const;
    virtual std::string toIsoString() const = 0;
};

} // namespace bas::time

#endif // BAS_TIME_ZONED_TIME_HPP
