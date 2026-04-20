#ifndef BAS_TIME_ZONE_ID_HPP
#define BAS_TIME_ZONE_ID_HPP

#include <cstdint>
#include <string>

namespace bas::time {

class ZoneId {
  public:
    explicit ZoneId(std::string value = "UTC");
    ~ZoneId() = default;
    std::string id() const;

  private:
    std::string m_value;
};

class ZoneOffset : public ZoneId {
  public:
    explicit ZoneOffset(int32_t seconds = 0);
    ~ZoneOffset() = default;
    int32_t totalSeconds() const;
    std::string id() const;

  private:
    int32_t m_seconds;
};

} // namespace bas::time

#endif // BAS_TIME_ZONE_ID_HPP
