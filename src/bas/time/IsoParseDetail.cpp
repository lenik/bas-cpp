#include "IsoParseDetail.hpp"

#include <date/date.h>
#include <date/tz.h>

#include <cctype>
#include <istream>
#include <sstream>

namespace bas::time::detail {
namespace {

bool tryParseUnsigned(std::string_view s, int& out) {
    if (s.empty()) {
        return false;
    }
    int value = 0;
    for (const char ch : s) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
        value = value * 10 + (ch - '0');
    }
    out = value;
    return true;
}

} // namespace

bool isStreamFullyConsumed(std::istream& in) {
    in >> std::ws;
    return in.peek() == std::char_traits<char>::eof();
}

std::string_view trimView(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

bool tryParseOffsetSeconds(std::string_view offset, int32_t& seconds) {
    offset = trimView(offset);
    if (offset.empty() || offset == "Z" || offset == "z") {
        seconds = 0;
        return true;
    }

    int sign = 1;
    if (offset.front() == '+') {
        offset.remove_prefix(1);
    } else if (offset.front() == '-') {
        sign = -1;
        offset.remove_prefix(1);
    } else {
        return false;
    }

    offset = trimView(offset);
    int hours = 0;
    int minutes = 0;
    const auto colon = offset.find(':');
    if (colon != std::string_view::npos) {
        if (!tryParseUnsigned(offset.substr(0, colon), hours)) {
            return false;
        }
        if (!tryParseUnsigned(offset.substr(colon + 1), minutes)) {
            return false;
        }
    } else if (offset.size() >= 4) {
        if (!tryParseUnsigned(offset.substr(0, 2), hours)) {
            return false;
        }
        if (!tryParseUnsigned(offset.substr(2, 2), minutes)) {
            return false;
        }
    } else if (!offset.empty()) {
        if (!tryParseUnsigned(offset, hours)) {
            return false;
        }
    } else {
        return false;
    }

    if (hours > 18 || minutes > 59) {
        return false;
    }

    seconds = static_cast<int32_t>(sign * (hours * 3600 + minutes * 60));
    return true;
}

ZoneSuffixParts splitZoneSuffix(std::string_view text) {
    const auto open = text.find('[');
    if (open == std::string_view::npos) {
        return {true, text, {}};
    }
    const auto close = text.find(']', open + 1);
    if (close == std::string_view::npos) {
        return {};
    }
    return {true, text.substr(0, open), text.substr(open + 1, close - open - 1)};
}

OffsetSuffixParts splitOffsetSuffix(std::string_view text) {
    const auto t = text.find('T');
    if (t == std::string_view::npos) {
        return {text, {}};
    }
    const auto off = text.find_first_of("+-", t);
    if (off == std::string_view::npos) {
        if (text.size() > t + 1 && (text.back() == 'Z' || text.back() == 'z')) {
            return {text.substr(0, text.size() - 1), "Z"};
        }
        return {text, {}};
    }
    return {text.substr(0, off), text.substr(off)};
}

OffsetSuffixParts splitOffsetTimeSuffix(std::string_view text) {
    const auto offPos = text.find_first_of("+-");
    if (offPos != std::string_view::npos) {
        return {text.substr(0, offPos), text.substr(offPos)};
    }
    if (!text.empty() && (text.back() == 'Z' || text.back() == 'z')) {
        return {text.substr(0, text.size() - 1), "Z"};
    }
    return {text, {}};
}

bool tryParseLocalDate(const std::string& text) {
    std::istringstream in(text);
    date::local_days value;
    in >> date::parse("%F", value);
    if (in.fail()) {
        return false;
    }
    return isStreamFullyConsumed(in);
}

bool tryParseLocalDateTimeLocal(std::string_view localPart) {
    std::istringstream in{std::string(localPart)};
    date::local_time<std::chrono::nanoseconds> value;
    in >> date::parse("%FT%T", value);
    if (in.fail()) {
        return false;
    }
    return isStreamFullyConsumed(in);
}

bool tryParseLocalDateTime(const std::string& text) {
    const auto parts = splitOffsetSuffix(text);
    if (!parts.offset.empty()) {
        return false;
    }
    return tryParseLocalDateTimeLocal(parts.local);
}

bool tryParseLocalTime(const std::string& text) {
    return tryParseLocalDateTimeLocal("1970-01-01T" + text);
}

bool tryParseInstant(const std::string& text) {
    std::istringstream in(text);
    date::sys_time<std::chrono::nanoseconds> value;
    if (text.find('Z') != std::string::npos || text.find('z') != std::string::npos) {
        in >> date::parse("%FT%TZ", value);
    } else if (text.find('T') != std::string::npos &&
               text.find_first_of("+-", text.find('T')) != std::string::npos) {
        in >> date::parse("%FT%T%Ez", value);
    } else {
        in >> date::parse("%FT%T", value);
    }
    if (in.fail()) {
        return false;
    }
    return isStreamFullyConsumed(in);
}

bool isKnownTimeZone(std::string_view zoneName) {
    if (zoneName.empty()) {
        return true;
    }
    return date::get_tzdb().locate_zone(std::string(zoneName)) != nullptr;
}

} // namespace bas::time::detail
