#ifndef BAS_TIME_ZONED_TIME_HPP
#define BAS_TIME_ZONED_TIME_HPP

#include "OffsetTime.hpp"

#include <memory>
#include <string>

namespace bas::time {

class ZonedTime : public Temporal {
  public:
    ~ZonedTime() override = default;

    virtual std::unique_ptr<OffsetTime> toOffsetTime() const = 0;
    virtual std::string zoneId() const = 0;
    virtual int compareTo(const ZonedTime& other) const;
    virtual std::string toIsoString() const override;

    static bool isValidIsoString(const std::string& text);
    static std::unique_ptr<ZonedTime> parseIsoString(const std::string& text);
};

} // namespace bas::time

#endif // BAS_TIME_ZONED_TIME_HPP
