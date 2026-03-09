#ifndef ASSETS_H
#define ASSETS_H

#include "../volume/MemoryZip.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

extern std::unique_ptr<MemoryZip> assets;

extern bool assets_contains(std::string_view path);

extern std::vector<uint8_t> assets_get_data(std::string_view path);

extern void assets_dump_tree();

#endif