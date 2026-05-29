#ifndef BAS_TIME_ISO_PARSE_DETAIL_HPP
#define BAS_TIME_ISO_PARSE_DETAIL_HPP

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace bas::time::detail {

bool isStreamFullyConsumed(std::istream& in);

std::string_view trimView(std::string_view s);

bool tryParseOffsetSeconds(std::string_view offset, int32_t& seconds);

struct ZoneSuffixParts {
    bool ok = false;
    std::string_view body;
    std::string_view zone;
};

ZoneSuffixParts splitZoneSuffix(std::string_view text);

struct OffsetSuffixParts {
    std::string_view local;
    std::string_view offset;
};

OffsetSuffixParts splitOffsetSuffix(std::string_view text);

OffsetSuffixParts splitOffsetTimeSuffix(std::string_view text);

bool tryParseLocalDate(const std::string& text);

bool tryParseLocalDateTimeLocal(std::string_view localPart);

bool tryParseLocalDateTime(const std::string& text);

bool tryParseLocalTime(const std::string& text);

bool tryParseInstant(const std::string& text);

bool isKnownTimeZone(std::string_view zoneName);

} // namespace bas::time::detail

#endif // BAS_TIME_ISO_PARSE_DETAIL_HPP
