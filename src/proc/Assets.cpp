#include "Assets.hpp"

#include "assets_data.h"

#include "../volume/FileStatus.hpp"
#include "../volume/MemoryZip.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

std::unique_ptr<MemoryZip> assets =
    std::make_unique<MemoryZip>(zip_data_start, zip_data_end - zip_data_start);

bool assets_contains(std::string_view path) { return assets->isFile(path); }

std::vector<uint8_t> assets_get_data(std::string_view path) {
    assert(assets.get());
    try {
        auto data = assets->readFile(path);
        return data;
    } catch (...) {
        return {};
    }
}

std::vector<std::unique_ptr<FileStatus>> assets_listdir(std::string_view dir) {
    assert(assets.get());

    std::vector<std::unique_ptr<FileStatus>> list = assets->readDir(dir, false);

    return list;
}

static void dump_dir(std::string_view path, const std::string& prefix) {
    assert(assets.get());
    std::vector<std::unique_ptr<FileStatus>> list;
    try {
        list = assets->readDir(path);
    } catch (const std::exception& e) {
        std::cerr << "Error reading directory " << path << ": " << e.what() << std::endl;
        return;
    }
    for (const auto& st : list) {
        if (!st)
            continue;
        bool isDir = st->isDirectory();
        std::string line = prefix;
        line += st->name;
        if (isDir)
            line += "/";
        if (st->isRegularFile() && st->size) {
            char buf[32];
            snprintf(buf, sizeof(buf), " (%zu)", static_cast<size_t>(st->size));
            line += buf;
        }
        std::puts(line.c_str());
        if (isDir) {
            std::string childPath(path);
            if (!childPath.empty())
                childPath += "/";
            childPath += st->name;
            dump_dir(childPath, prefix + "  ");
        }
    }
}

void assets_dump_tree() { dump_dir("/", ""); }