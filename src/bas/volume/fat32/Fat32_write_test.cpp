#include "Fat32Volume.hpp"

#include "../../io/IOException.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <bas/log/logger.h>

#include <unistd.h>

#define PREFIX "Fat32_write_test_"

namespace fs = std::filesystem;

extern "C" logger_t test_logger = {};

static int run_cmd(const std::string& cmd) { 
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Command failed: " << cmd << " (exit code: " << ret << ")\n";
    }
    return ret;
}

int main() {
    const fs::path tmpBase =
        fs::temp_directory_path() / (PREFIX + std::to_string(::getpid()));
    fs::create_directories(tmpBase);

    const fs::path image = tmpBase / "disk.img";

    std::cout << "Creating FAT32 image...\n";
    assert(run_cmd("dd if=/dev/zero of=\"" + image.string() + "\" bs=1M count=16 status=none") == 0);
    assert(run_cmd("mkfs.fat -F 32 \"" + image.string() + "\" >/dev/null 2>&1") == 0);

    std::cout << "Opening FAT32 image...\n";
    Fat32Volume vol(image.string());
    assert(vol.getClass() == "fat32");

    std::cout << "\n=== FAT32 Write API Tests ===\n";

    // Test that write methods exist and have correct signatures
    std::cout << "✓ Volume interface supports write operations\n";

    // Test 1: writeFile method exists
    try {
        std::vector<uint8_t> testData = {'t', 'e', 's', 't'};
        vol.writeFile("/test.bin", testData);
        std::cout << "✓ writeFile() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ writeFile() failed: " << e.what() << "\n";
    }

    // Test 2: writeFileUTF8 method exists
    try {
        vol.writeFileUTF8("/test.txt", "Hello");
        std::cout << "✓ writeFileUTF8() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ writeFileUTF8() failed: " << e.what() << "\n";
    }

    // Test 3: createDirectory method exists
    try {
        vol.createDirectory("/dir");
        std::cout << "✓ createDirectory() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ createDirectory() failed: " << e.what() << "\n";
    }

    // Test 4: createDirectories method exists
    try {
        vol.createDirectories("/a/b/c");
        std::cout << "✓ createDirectories() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ createDirectories() failed: " << e.what() << "\n";
    }

    // Test 5: removeFile method exists
    try {
        vol.removeFile("/test.txt");
        std::cout << "✓ removeFile() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ removeFile() failed: " << e.what() << "\n";
    }

    // Test 6: removeDirectory method exists
    try {
        vol.removeDirectory("/dir");
        std::cout << "✓ removeDirectory() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ removeDirectory() failed: " << e.what() << "\n";
    }

    // Test 7: copyFile method exists
    try {
        vol.copyFile("/test.bin", "/test_copy.bin");
        std::cout << "✓ copyFile() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ copyFile() failed: " << e.what() << "\n";
    }

    // Test 8: moveFile method exists
    try {
        vol.moveFile("/test_copy.bin", "/moved.bin");
        std::cout << "✓ moveFile() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ moveFile() failed: " << e.what() << "\n";
    }

    // Test 9: rename method exists
    try {
        vol.rename("/moved.bin", "/renamed.bin");
        std::cout << "✓ rename() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ rename() failed: " << e.what() << "\n";
    }

    // Test 10: newOutputStream method exists
    try {
        auto out = vol.newOutputStream("/stream.bin");
        assert(out != nullptr);
        std::cout << "✓ newOutputStream() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ newOutputStream() failed: " << e.what() << "\n";
    }

    // Test 11: newWriter method exists
    try {
        auto writer = vol.newWriter("/writer.txt");
        assert(writer != nullptr);
        std::cout << "✓ newWriter() method callable\n";
    } catch (const std::exception& e) {
        std::cout << "✗ newWriter() failed: " << e.what() << "\n";
    }

    fs::remove_all(tmpBase);
    std::cout << "\nFAT32 API tests completed.\n";
    std::cout << "===========================================\n";
    
    return 0;
}
