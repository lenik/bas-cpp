#include "Fat32Volume.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <bas/log/logger.h>

#include <unistd.h>

namespace fs = std::filesystem;

extern "C" logger_t test_logger = {};

static int run_cmd(const std::string& cmd) { return std::system(cmd.c_str()); }

int main() {
    const fs::path tmpBase =
        fs::temp_directory_path() / ("Fat32Volume_test_" + std::to_string(::getpid()));
    fs::create_directories(tmpBase);

    const fs::path image = tmpBase / "disk.img";
    const fs::path host = tmpBase / "hello.txt";

    {
        std::ofstream out(host, std::ios::binary);
        out << "hello-fat32";
    }

    assert(run_cmd("dd if=/dev/zero of=\"" + image.string() + "\" bs=1M count=8 status=none") == 0);
    assert(run_cmd("mkfs.fat -F 32 \"" + image.string() + "\" >/dev/null 2>&1") == 0);
    assert(run_cmd("mcopy -i \"" + image.string() + "\" \"" + host.string() + "\" ::HELLO.TXT") ==
           0);

    Fat32Volume vol(image.string());
    assert(vol.getClass() == "fat32");

    bool foundHello = false;
    auto node = vol.readDir("/", false);
    for (const auto& [name, child] : node->children) {
        if (child->name == "hello.txt" || child->name == "HELLO.TXT" || child->name == "hello") {
            foundHello = true;
            break;
        }
    }
    assert(foundHello);

    const auto data = vol.readFile("/hello.txt");
    const std::string s(data.begin(), data.end());
    assert(s == "hello-fat32");

    fs::remove_all(tmpBase);
    std::cout << "All Fat32Volume tests passed.\n";
    return 0;
}
