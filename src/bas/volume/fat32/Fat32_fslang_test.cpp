#include "Fat32Volume.hpp"
#include "../../io/BlockDevice.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <bas/log/logger.h>

#include <unistd.h>

#define PREFIX "Fat32_fslang_test_"

namespace fs = std::filesystem;

extern "C" logger_t test_logger = {};

static int run_cmd(const std::string& cmd) {
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Command failed: " << cmd << " (exit code: " << ret << ")\n";
    }
    return ret;
}

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

int main() {
    const fs::path tmpBase = fs::temp_directory_path() / (PREFIX + std::to_string(::getpid()));
    fs::create_directories(tmpBase);
    
    const fs::path image = tmpBase / "disk.img";
    const fs::path testDir = fs::path(TESTS_DIR) / "fslang";
    
    std::cout << "=== FAT32 FSLang Test Suite ===\n\n";
    
    // Create FAT32 image
    std::cout << "Creating FAT32 image...\n";
    assert(run_cmd("dd if=/dev/zero of=\"" + image.string() + "\" bs=1M count=16 status=none") == 0);
    assert(run_cmd("mkfs.fat -F 32 \"" + image.string() + "\" >/dev/null 2>&1") == 0);
    
    // Load image into memory
    std::cout << "Loading FAT32 image into memory...\n";
    std::ifstream inFile(image.string(), std::ios::binary | std::ios::ate);
    size_t imageSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    std::vector<uint8_t> imageBuffer(imageSize);
    inFile.read(reinterpret_cast<char*>(imageBuffer.data()), imageSize);
    inFile.close();
    
    int testsPassed = 0;
    int testsFailed = 0;
    
    // Run basic test
    {
        std::cout << "\n--- Running fat32_basic.fsl ---\n";
        
        auto device = createMemDevice(imageBuffer.data(), imageSize);
        Fat32Volume vol(device);
        
        std::string script = readFile((testDir / "fat32_basic.fsl").string());
        
        int commitCount = 0;
        auto commitCallback = [&commitCount]() {
            commitCount++;
        };
        
        auto result = vol.run(script, commitCallback);
        
        if (result.success) {
            std::cout << "✓ fat32_basic.fsl PASSED (" << commitCount << " commits)\n";
            testsPassed++;
        } else {
            std::cerr << "✗ fat32_basic.fsl FAILED at line " << result.failedLine << "\n";
            std::cerr << "  Error: " << result.error << "\n";
            testsFailed++;
        }
    }
    
    // Run stress test
    {
        std::cout << "\n--- Running fat32_stress.fsl ---\n";
        
        // Reset image
        std::ifstream inFile2(image.string(), std::ios::binary | std::ios::ate);
        size_t imageSize2 = inFile2.tellg();
        inFile2.seekg(0, std::ios::beg);
        std::vector<uint8_t> imageBuffer2(imageSize2);
        inFile2.read(reinterpret_cast<char*>(imageBuffer2.data()), imageSize2);
        inFile2.close();
        
        auto device = createMemDevice(imageBuffer2.data(), imageSize2);
        Fat32Volume vol(device);
        
        std::string script = readFile((testDir / "fat32_stress.fsl").string());
        
        int commitCount = 0;
        auto commitCallback = [&commitCount]() {
            commitCount++;
        };
        
        auto result = vol.run(script, commitCallback);
        
        if (result.success) {
            std::cout << "✓ fat32_stress.fsl PASSED (" << commitCount << " commits)\n";
            testsPassed++;
        } else {
            std::cerr << "✗ fat32_stress.fsl FAILED at line " << result.failedLine << "\n";
            std::cerr << "  Error: " << result.error << "\n";
            testsFailed++;
        }
    }
    
    // Write final image back for inspection
    std::cout << "\nWriting final image to disk for inspection...\n";
    std::ofstream outFile(image.string(), std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(imageBuffer.data()), imageSize);
    outFile.close();
    
    fs::remove_all(tmpBase);
    
    std::cout << "\n===========================================\n";
    std::cout << "FAT32 FSLang Test Suite Complete\n";
    std::cout << "  Passed: " << testsPassed << "\n";
    std::cout << "  Failed: " << testsFailed << "\n";
    std::cout << "===========================================\n";
    
    return (testsFailed == 0) ? 0 : 1;
}
