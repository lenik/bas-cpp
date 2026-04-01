#include "fat32_utils.hpp"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

std::string baseName(std::string_view path) {
    if (path.empty() || path == "/") {
        return "/";
    }
    size_t pos = path.find_last_of('/');
    if (pos == std::string_view::npos || pos + 1 >= path.size()) {
        return std::string(path);
    }
    return std::string(path.substr(pos + 1));
}

// Calculate checksum for LFN entries
uint8_t calculateLFNChecksum(const uint8_t* shortName) {
    uint8_t sum = 0;
    for (int i = 11; i >= 0; --i) {
        sum = ((sum & 1) << 7) + (sum >> 1) + shortName[i];
    }
    return sum;
}

uint8_t checksumShortName(const uint8_t* shortEntry) {
    uint8_t sum = 0;
    for (int i = 11; i >= 0; --i) {
        sum = ((sum & 1) << 7) + (sum >> 1) + shortEntry[i];
    }
    return sum;
}

// Encode DOS date/time from time_t
void encodeDosDateTime(time_t t, uint16_t* dosTime, uint16_t* dosDate) {
    struct tm* tm = localtime(&t);
    if (!tm) {
        *dosTime = 0;
        *dosDate = 0;
        return;
    }
    *dosTime = static_cast<uint16_t>((tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2));
    *dosDate = static_cast<uint16_t>(((tm->tm_year + 1900 - 1980) << 9) | ((tm->tm_mon + 1) << 5) |
                                     tm->tm_mday);
}

void encodeLfnEntry(const std::string& name, uint8_t order, uint8_t* entry) {
    memset(entry, 0xFF, 32);
    entry[11] = 0x0F; // LFN attribute
    entry[0] = order;
    entry[26] = 0; // Checksum (will be set later)

    // Encode name as UTF-16LE
    size_t charIdx = 0;
    auto writeChar = [&](int offset) {
        if (charIdx < name.size()) {
            uint16_t ch = static_cast<unsigned char>(name[charIdx++]);
            entry[offset] = ch & 0xFF;
            entry[offset + 1] = (ch >> 8) & 0xFF;
        } else {
            entry[offset] = 0xFF;
            entry[offset + 1] = 0xFF;
        }
    };

    for (int i = 0; i < 5; ++i)
        writeChar(1 + i * 2);
    for (int i = 0; i < 6; ++i)
        writeChar(14 + i * 2);
    for (int i = 0; i < 2; ++i)
        writeChar(28 + i * 2);
}

void encodeShortName(const std::string& name, uint8_t* entry) {
    memset(entry, ' ', 11);

    std::string upperName = name;
    for (char& c : upperName) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    size_t dotPos = upperName.find('.');
    if (dotPos == std::string::npos) {
        // No extension
        for (size_t i = 0; i < std::min<size_t>(8, upperName.size()); ++i) {
            entry[i] = upperName[i];
        }
    } else {
        // Has extension
        for (size_t i = 0; i < std::min<size_t>(8, dotPos); ++i) {
            entry[i] = upperName[i];
        }
        for (size_t i = 0; i < std::min<size_t>(3, upperName.size() - dotPos - 1); ++i) {
            entry[8 + i] = upperName[dotPos + 1 + i];
        }
    }
}

time_t getCurrentTime() { return time(nullptr); }

/** FAT directory 32-byte entry uses MS-DOS date/time encoding (same as ZIP). */
time_t fatDosDateTime(uint16_t dosTime, uint16_t dosDate) {
    if (dosTime == 0 && dosDate == 0)
        return 0;
    int sec = (dosTime & 0x1F) * 2;
    int min = (dosTime >> 5) & 0x3F;
    int hour = (dosTime >> 11) & 0x1F;
    int day = dosDate & 0x1F;
    int month = (dosDate >> 5) & 0x0F;
    int year = (dosDate >> 9) + 1980;
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 59)
        return 0;
    struct tm tm = {};
    tm.tm_sec = sec;
    tm.tm_min = min;
    tm.tm_hour = hour;
    tm.tm_mday = day;
    tm.tm_mon = month - 1;
    tm.tm_year = year - 1900;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

bool isDotEntry(std::string_view name) { return name == "." || name == ".."; }

uint16_t le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
std::string trimRight(const std::string& s) {
    size_t end = s.size();
    while (end > 0 && s[end - 1] == ' ')
        --end;
    return s.substr(0, end);
}

// Write a single 32-byte directory entry
void writeDirEntry(uint8_t* buffer, const std::string& name, uint32_t firstCluster, uint32_t size,
                   bool isDir, time_t mtime, time_t atime, time_t ctime) {
    memset(buffer, 0, 32);

    // Encode short name (8.3 format)
    std::string shortName = name;
    std::string ext = "";

    size_t dotPos = shortName.find_last_of('.');
    if (dotPos != std::string::npos && dotPos < shortName.size() - 1) {
        ext = shortName.substr(dotPos + 1);
        shortName = shortName.substr(0, dotPos);
    }

    // Convert to uppercase and pad
    std::string shortNameUpper = shortName;
    std::string extUpper = ext;
    for (char& c : shortNameUpper)
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    for (char& c : extUpper)
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

    // Name (8 chars)
    for (int i = 0; i < 8; ++i) {
        buffer[i] = (i < static_cast<int>(shortNameUpper.size())) ? shortNameUpper[i] : ' ';
    }

    // Extension (3 chars)
    for (int i = 0; i < 3; ++i) {
        buffer[8 + i] = (i < static_cast<int>(extUpper.size())) ? extUpper[i] : ' ';
    }

    // Attributes (0x10 = directory, 0x20 = archive for files)
    buffer[11] = isDir ? 0x10 : 0x20;

    // Reserved byte
    buffer[12] = 0;

    // Creation time (tenths of seconds - set to 0)
    buffer[13] = 0;

    // Creation time
    uint16_t dosCreateTime;
    uint16_t dosCreateDate;
    encodeDosDateTime(ctime, &dosCreateTime, &dosCreateDate);
    buffer[14] = dosCreateTime & 0xFF;
    buffer[15] = (dosCreateTime >> 8) & 0xFF;
    buffer[16] = dosCreateDate & 0xFF;
    buffer[17] = (dosCreateDate >> 8) & 0xFF;

    // Last access date
    uint16_t dosAccDate;
    encodeDosDateTime(atime, &dosCreateTime, &dosAccDate);
    buffer[18] = dosAccDate & 0xFF;
    buffer[19] = (dosAccDate >> 8) & 0xFF;

    // First cluster high (FAT32)
    buffer[20] = (firstCluster >> 16) & 0xFF;
    buffer[21] = (firstCluster >> 24) & 0xFF;

    // Last write time
    uint16_t dosModTime;
    uint16_t dosModDate;
    encodeDosDateTime(mtime, &dosModTime, &dosModDate);
    buffer[22] = dosModTime & 0xFF;
    buffer[23] = (dosModTime >> 8) & 0xFF;
    buffer[24] = dosModDate & 0xFF;
    buffer[25] = (dosModDate >> 8) & 0xFF;

    // First cluster low
    buffer[26] = firstCluster & 0xFF;
    buffer[27] = (firstCluster >> 8) & 0xFF;

    // File size
    buffer[28] = size & 0xFF;
    buffer[29] = (size >> 8) & 0xFF;
    buffer[30] = (size >> 16) & 0xFF;
    buffer[31] = (size >> 24) & 0xFF;
}
