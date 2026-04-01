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

#define PREFIX "Ext4_test_"

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

    std::cout << "Creating ext4 image...\n";
    assert(run_cmd("dd if=/dev/zero of=\"" + image.string() + "\" bs=1M count=32 status=none") == 0);
    assert(run_cmd("mkfs.ext4 -F \"" + image.string() + "\" >/dev/null 2>&1") == 0);

    std::cout << "Mounting ext4 image...\n";
    Ext4Volume vol(image.string());
    assert(vol.getClass() == "ext");

    // Test write operations
    std::cout << "Testing ext4 write operations...\n";

    try {
        // Test 1: writeFile - write a new file
        {
            std::cout << "  Test 1: writeFile... ";
            std::vector<uint8_t> testData = {'H', 'e', 'l', 'l', 'o', ' ', 'E', 'X', 'T', '4', '!'};
            vol.writeFile("/test_write.txt", testData);
            
            // Verify by reading back
            auto readData = vol.readFile("/test_write.txt");
            assert(readData == testData);
            std::cout << "PASSED\n";
        }

        // Test 2: writeFileUTF8 - write text file
        {
            std::cout << "  Test 2: writeFileUTF8... ";
            vol.writeFileUTF8("/test_text.txt", "Hello World!\n");
            auto content = vol.readFileUTF8("/test_text.txt");
            assert(content == "Hello World!\n");
            std::cout << "PASSED\n";
        }

        // Test 3: createDirectory
        {
            std::cout << "  Test 3: createDirectory... ";
            vol.createDirectory("/test_dir");
            assert(vol.exists("/test_dir"));
            assert(vol.isDirectory("/test_dir"));
            std::cout << "PASSED\n";
        }

        // Test 4: createDirectories (nested)
        {
            std::cout << "  Test 4: createDirectories... ";
            vol.createDirectories("/test_dir/nested/deep");
            assert(vol.exists("/test_dir/nested/deep"));
            assert(vol.isDirectory("/test_dir/nested/deep"));
            std::cout << "PASSED\n";
        }

        // Test 5: write file in subdirectory
        {
            std::cout << "  Test 5: writeFile in subdirectory... ";
            std::vector<uint8_t> subData = {'S', 'u', 'b', 'd', 'i', 'r', ' ', 'f', 'i', 'l', 'e'};
            vol.writeFile("/test_dir/nested/subfile.bin", subData);
            auto readData = vol.readFile("/test_dir/nested/subfile.bin");
            assert(readData == subData);
            std::cout << "PASSED\n";
        }

        // Test 6: copyFile
        {
            std::cout << "  Test 6: copyFile... ";
            vol.copyFile("/test_write.txt", "/test_copy.txt");
            assert(vol.exists("/test_copy.txt"));
            auto original = vol.readFile("/test_write.txt");
            auto copy = vol.readFile("/test_copy.txt");
            assert(original == copy);
            std::cout << "PASSED\n";
        }

        // Test 7: removeFile
        {
            std::cout << "  Test 7: removeFile... ";
            assert(vol.exists("/test_copy.txt"));
            vol.removeFile("/test_copy.txt");
            assert(!vol.exists("/test_copy.txt"));
            std::cout << "PASSED\n";
        }

        // Test 8: removeDirectory (empty)
        {
            std::cout << "  Test 8: removeDirectory (empty)... ";
            vol.createDirectory("/empty_dir");
            vol.removeDirectory("/empty_dir");
            assert(!vol.exists("/empty_dir"));
            std::cout << "PASSED\n";
        }

        // Test 9: renameFile
        {
            std::cout << "  Test 9: renameFile... ";
            vol.rename("/test_write.txt", "/test_renamed.txt");
            assert(!vol.exists("/test_write.txt"));
            assert(vol.exists("/test_renamed.txt"));
            std::cout << "PASSED\n";
        }

        // Test 10: moveFile
        {
            std::cout << "  Test 10: moveFile... ";
            vol.moveFile("/test_renamed.txt", "/test_dir/moved.txt");
            assert(!vol.exists("/test_renamed.txt"));
            assert(vol.exists("/test_dir/moved.txt"));
            std::cout << "PASSED\n";
        }

        // Test 11: Write larger file
        {
            std::cout << "  Test 11: writeFile (large)... ";
            std::vector<uint8_t> largeData(8192); // 8KB
            for (size_t i = 0; i < largeData.size(); ++i) {
                largeData[i] = static_cast<uint8_t>(i % 256);
            }
            vol.writeFile("/large_file.bin", largeData);
            auto readData = vol.readFile("/large_file.bin");
            assert(readData == largeData);
            std::cout << "PASSED\n";
        }

        // Test 12: List directory contents
        {
            std::cout << "  Test 12: readDir... ";
            auto dir = vol.readDir("/test_dir", true);
            assert(dir->children.size() > 0);
            std::cout << "PASSED\n";
        }

        // Test 13: File metadata (stat)
        {
            std::cout << "  Test 13: stat... ";
            DirNode status;
            assert(vol.stat("/test_text.txt", &status));
            assert(status.size > 0);
            std::cout << "PASSED\n";
        }

        std::cout << "\nAll ext4 write tests PASSED!\n";

    } catch (const std::exception& e) {
        std::cerr << "\nTest FAILED with exception: " << e.what() << "\n";
        fs::remove_all(tmpBase);
        return 1;
    }

    fs::remove_all(tmpBase);
    return 0;
}
