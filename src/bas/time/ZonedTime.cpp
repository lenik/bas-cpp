#include "ZonedTime.hpp"

namespace bas::time {

int ZonedTime::compareTo(const ZonedTime& other) const {
    return toOffsetTime()->compareTo(*other.toOffsetTime());
}

} // namespace bas::time
