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

#define PREFIX "Fat32_persistence_test_"

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

    // Load image into memory
    std::cout << "Loading FAT32 image into memory...\n";
    std::ifstream inFile(image.string(), std::ios::binary | std::ios::ate);
    size_t imageSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    std::vector<uint8_t> imageBuffer(imageSize);
    inFile.read(reinterpret_cast<char*>(imageBuffer.data()), imageSize);
    inFile.close();

    std::cout << "=== FAT32 Persistence Test (Memory-Backed) ===\n\n";

    // Phase 1: Write data
    std::cout << "Phase 1: Writing data to FAT32 image...\n";
    {
        auto device = createMemDevice(imageBuffer.data(), imageSize);
        Fat32Volume vol(device);
        
        // Write several files
        vol.writeFileUTF8("/file1.txt", "Hello from FAT32!\n");
        vol.writeFileUTF8("/file2.txt", "Second file content\n");
        vol.createDirectory("/subdir");
        vol.writeFileUTF8("/subdir/nested.txt", "Nested file content\n");
        
        std::cout << "  ✓ Wrote /file1.txt\n";
        std::cout << "  ✓ Wrote /file2.txt\n";
        std::cout << "  ✓ Created /subdir\n";
        std::cout << "  ✓ Wrote /subdir/nested.txt\n";
    }

    // Phase 2: Remount and verify (create new MemDevice from same buffer)
    std::cout << "\nPhase 2: Remounting and verifying data...\n";
    {
        auto device = createMemDevice(imageBuffer.data(), imageSize);
        Fat32Volume vol(device);
        
        // Verify files exist
        assert(vol.exists("/file1.txt"));
        std::cout << "  ✓ /file1.txt exists\n";
        
        assert(vol.exists("/file2.txt"));
        std::cout << "  ✓ /file2.txt exists\n";
        
        assert(vol.exists("/subdir"));
        assert(vol.isDirectory("/subdir"));
        std::cout << "  ✓ /subdir exists and is a directory\n";
        
        assert(vol.exists("/subdir/nested.txt"));
        std::cout << "  ✓ /subdir/nested.txt exists\n";
        
        // Verify content
        auto content1 = vol.readFileUTF8("/file1.txt");
        assert(content1 == "Hello from FAT32!\n");
        std::cout << "  ✓ /file1.txt content verified\n";
        
        auto content2 = vol.readFileUTF8("/file2.txt");
        assert(content2 == "Second file content\n");
        std::cout << "  ✓ /file2.txt content verified\n";
        
        auto content3 = vol.readFileUTF8("/subdir/nested.txt");
        assert(content3 == "Nested file content\n");
        std::cout << "  ✓ /subdir/nested.txt content verified\n";
        
        // List directory
        auto dir = vol.readDir("/", false);
        std::cout << "  ✓ Root directory listing: " << dir->children.size() << " entries\n";
        
        auto subdir = vol.readDir("/subdir", false);
        std::cout << "  ✓ /subdir listing: " << subdir->children.size() << " entries\n";
    }

    // Phase 3: Modify data
    std::cout << "\nPhase 3: Modifying data...\n";
    {
        auto device = createMemDevice(imageBuffer.data(), imageSize);
        Fat32Volume vol(device);
        
        // Update a file
        vol.writeFileUTF8("/file1.txt", "Updated content!\n");
        std::cout << "  ✓ Updated /file1.txt\n";
        
        // Add new file
        vol.writeFileUTF8("/file3.txt", "Third file\n");
        std::cout << "  ✓ Created /file3.txt\n";
        
        // Remove a file
        vol.removeFile("/file2.txt");
        std::cout << "  ✓ Removed /file2.txt\n";
        
        // Copy file
        vol.copyFile("/file3.txt", "/file3_copy.txt");
        std::cout << "  ✓ Copied /file3.txt to /file3_copy.txt\n";
    }

    // Phase 4: Verify modifications
    std::cout << "\nPhase 4: Verifying modifications...\n";
    {
        auto device = createMemDevice(imageBuffer.data(), imageSize);
        Fat32Volume vol(device);
        
        // Verify update
        auto content1 = vol.readFileUTF8("/file1.txt");
        assert(content1 == "Updated content!\n");
        std::cout << "  ✓ /file1.txt update verified\n";
        
        // Verify new file
        assert(vol.exists("/file3.txt"));
        std::cout << "  ✓ /file3.txt exists\n";
        
        // Verify removal
        assert(!vol.exists("/file2.txt"));
        std::cout << "  ✓ /file2.txt removed\n";
        
        // Verify copy
        assert(vol.exists("/file3_copy.txt"));
        auto copyContent = vol.readFileUTF8("/file3_copy.txt");
        assert(copyContent == "Third file\n");
        std::cout << "  ✓ /file3_copy.txt exists with correct content\n";
        
        // List all files
        std::cout << "\nFinal directory structure:\n";
        auto dir = vol.readDir("/", true);
        for (const auto& [name, child] : dir->children) {
            std::cout << "  " << (child->type == FileType::Directory ? "[DIR]  " : "[FILE] ");
            std::cout << name << "\n";
        }
    }

    // Write memory back to file for inspection
    std::cout << "\nWriting memory image back to disk...\n";
    std::ofstream outFile(image.string(), std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(imageBuffer.data()), imageSize);
    outFile.close();
    
    fs::remove_all(tmpBase);
    
    std::cout << "\n===========================================\n";
    std::cout << "All persistence tests PASSED!\n";
    std::cout << "Data survives remounting - memory writes work!\n";
    std::cout << "===========================================\n";
    
    return 0;
}
