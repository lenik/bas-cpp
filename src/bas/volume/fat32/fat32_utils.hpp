#ifndef FAT32_UTILS_HPP
#define FAT32_UTILS_HPP

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

std::string baseName(std::string_view path);

uint8_t checksumShortName(const uint8_t* shortEntry);

uint8_t calculateLFNChecksum(const uint8_t* shortName);

void encodeDosDateTime(time_t t, uint16_t* dosTime, uint16_t* dosDate);

void encodeLfnEntry(const std::string& name, uint8_t order, uint8_t* entry);

void encodeShortName(const std::string& name, uint8_t* entry);

time_t fatDosDateTime(uint16_t dosTime, uint16_t dosDate);

time_t getCurrentTime();

bool isDotEntry(std::string_view name);

uint16_t le16(const uint8_t* p);

uint32_t le32(const uint8_t* p);

std::string trimRight(const std::string& s);

void writeDirEntry(uint8_t* buffer, const std::string& name, uint32_t firstCluster, uint32_t size,
                   bool isDir, time_t mtime, time_t atime, time_t ctime);

#endif // FAT32_UTILS_HPP
