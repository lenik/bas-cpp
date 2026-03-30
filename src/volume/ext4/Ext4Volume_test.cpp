#include "Ext4Volume.hpp"

#include "../../io/IOException.hpp"

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

static int run_cmd(const std::string& cmd) {
    return std::system(cmd.c_str());
}

int main() {
    const fs::path tmpBase = fs::temp_directory_path() / ("Ext4Volume_test_" + std::to_string(::getpid()));
    fs::create_directories(tmpBase);

    const fs::path image = tmpBase / "disk.img";
    const fs::path host = tmpBase / "hello.txt";

    {
        std::ofstream out(host, std::ios::binary);
        out << "hello-ext4";
    }

    assert(run_cmd("dd if=/dev/zero of=\"" + image.string() + "\" bs=1M count=16 status=none") == 0);
    assert(run_cmd("mkfs.ext4 -F \"" + image.string() + "\" >/dev/null 2>&1") == 0);
    assert(run_cmd("e2cp \"" + host.string() + "\" \"" + image.string() + "\":/hello.txt") == 0);

#if defined(BAS_HAS_EXT2FS) && BAS_HAS_EXT2FS
    Ext4Volume vol(image.string());
    assert(vol.getClass() == "ext");
    assert(vol.exists("/hello.txt"));
    assert(vol.isFile("/hello.txt"));
    const auto data = vol.readFile("/hello.txt");
    const std::string s(data.begin(), data.end());
    assert(s == "hello-ext4");
#else
    bool threw = false;
    try {
        Ext4Volume vol(image.string());
    } catch (const IOException&) {
        threw = true;
    }
    assert(threw);
#endif

    fs::remove_all(tmpBase);
    std::cout << "All Ext4Volume tests passed.\n";
    return 0;
}
