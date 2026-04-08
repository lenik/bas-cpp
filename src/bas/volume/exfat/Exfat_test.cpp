#include "ExfatVolume.hpp"
#include "../../io/BlockDevice.hpp"
#include "../../io/IOException.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <bas/log/logger.h>

#include <unistd.h>

#define PREFIX "Exfat_test_"

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
    const fs::path tmpBase = fs::temp_directory_path() / (PREFIX + std::to_string(::getpid()));
    fs::create_directories(tmpBase);

    const fs::path image = tmpBase / "disk.img";

    std::cout << "Creating exFAT image...\n";
    assert(run_cmd("dd if=/dev/zero of=\"" + image.string() + "\" bs=1M count=32 status=none") == 0);
    assert(run_cmd("mkfs.exfat -F \"" + image.string() + "\" >/dev/null 2>&1") == 0);

    std::cout << "Loading exFAT image into memory...\n";
    std::ifstream inFile(image.string(), std::ios::binary | std::ios::ate);
    size_t imageSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    std::vector<uint8_t> imageBuffer(imageSize);
    inFile.read(reinterpret_cast<char*>(imageBuffer.data()), imageSize);
    inFile.close();

    std::cout << "Mounting exFAT from memory device...\n";
    auto device = createMemDevice(imageBuffer.data(), imageSize);
    ExfatVolume vol(device);
    assert(vol.getClass() == "exfat");

    // Test read operations (write not yet implemented)
    std::cout << "Testing exFAT read operations...\n";

    try {
        // Test 1: Root directory exists
        {
            std::cout << "  Test 1: Root directory exists... ";
            assert(vol.exists("/"));
            assert(vol.isDirectory("/"));
            std::cout << "PASSED\n";
        }

        // Test 2: Read directory listing
        {
            std::cout << "  Test 2: readDir root... ";
            auto dir = vol.readDir("/", false);
            assert(dir != nullptr);
            std::cout << "PASSED (" << dir->children.size() << " entries)\n";
        }

        // Test 3: stat on root
        {
            std::cout << "  Test 3: stat root... ";
            DirNode status;
            assert(vol.stat("/", &status));
            assert(status.type == FileType::Directory);
            std::cout << "PASSED\n";
        }

        // Test 4: getTempDir throws (not supported)
        {
            std::cout << "  Test 4: getTempDir not supported... ";
            try {
                vol.getTempDir();
                std::cout << "FAILED (should throw)\n";
                assert(false);
            } catch (const IOException&) {
                std::cout << "PASSED\n";
            }
        }

        // Test 5: Write operations throw (not implemented)
        {
            std::cout << "  Test 5: Write operations throw... ";
            try {
                std::vector<uint8_t> data = {'t', 'e', 's', 't'};
                vol.writeFile("/test.txt", data);
                std::cout << "FAILED (should throw)\n";
                assert(false);
            } catch (const IOException& e) {
                std::string msg = e.what();
                if (msg.find("not yet implemented") != std::string::npos ||
                    msg.find("not implemented") != std::string::npos) {
                    std::cout << "PASSED\n";
                } else {
                    std::cout << "FAILED (wrong error: " << msg << ")\n";
                    assert(false);
                }
            }
        }

        // Test 6: Create directory throws (not implemented)
        {
            std::cout << "  Test 6: createDirectory throws... ";
            try {
                vol.createDirectory("/test_dir");
                std::cout << "FAILED (should throw)\n";
                assert(false);
            } catch (const IOException&) {
                std::cout << "PASSED\n";
            }
        }

        std::cout << "\nAll exFAT read tests PASSED!\n";
        std::cout << "Note: Write operations are stubbed and throw 'not implemented'\n";

    } catch (const std::exception& e) {
        std::cerr << "\nTest FAILED with exception: " << e.what() << "\n";
        fs::remove_all(tmpBase);
        return 1;
    }

    // Write memory back to file for inspection
    std::cout << "\nWriting memory image back to disk...\n";
    std::ofstream outFile(image.string(), std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(imageBuffer.data()), imageSize);
    outFile.close();

    fs::remove_all(tmpBase);
    return 0;
}
