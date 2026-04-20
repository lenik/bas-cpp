#include "ZonedDateTime.hpp"

namespace bas::time {

int ZonedDateTime::compareTo(const ZonedDateTime& other) const {
    return toOffsetDateTime()->compareTo(*other.toOffsetDateTime());
}

} // namespace bas::time
