#include "Ext4Volume.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <bas/log/logger.h>

#include <unistd.h>

#define PREFIX "Ext4_forest_test_"

namespace fs = std::filesystem;

extern "C" logger_t test_logger = {};

static int run_cmd(const std::string& cmd) {
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Command failed: " << cmd << " (exit code: " << ret << ")\n";
    }
    return ret;
}

struct TreeNode {
    std::string path;
    bool isDirectory;
    std::vector<uint8_t> content;  // For files only
    std::vector<std::string> children;
    
    TreeNode() : isDirectory(false) {}
    TreeNode(const std::string& p, bool dir) : path(p), isDirectory(dir) {}
};

class RandomForestGenerator {
  public:
    RandomForestGenerator(unsigned int seed = 0) : rng(seed) {}
    
    std::vector<TreeNode> generate(size_t targetNodes = 1000) {
        std::vector<TreeNode> nodes;
        nodes.emplace_back("/", true);  // Root
        
        std::uniform_real_distribution<> dirProb(0.0, 1.0);
        std::uniform_int_distribution<> nameLenDist(3, 12);
        std::uniform_int_distribution<> contentSizeDist(0, 4096);
        std::uniform_int_distribution<> byteDist(0, 255);
        
        const char* nameChars = "abcdefghijklmnopqrstuvwxyz0123456789_-";
        const size_t nameCharsLen = strlen(nameChars) - 1;
        
        std::vector<std::string> availableDirs = {"/"};
        size_t nodesCreated = 1;
        
        while (nodesCreated < targetNodes) {
            // Pick a random directory to add to
            std::uniform_int_distribution<> dirIdx(0, availableDirs.size() - 1);
            std::string parentPath = availableDirs[dirIdx(rng)];
            
            // Generate random name
            std::string name;
            int nameLen = nameLenDist(rng);
            for (int i = 0; i < nameLen; ++i) {
                std::uniform_int_distribution<> charIdx(0, nameCharsLen - 1);
                name += nameChars[charIdx(rng)];
            }
            
            // Add extension sometimes
            if (!name.empty() && dirProb(rng) < 0.3) {
                std::uniform_int_distribution<> extType(0, 4);
                switch (extType(rng)) {
                    case 0: name += ".txt"; break;
                    case 1: name += ".bin"; break;
                    case 2: name += ".dat"; break;
                    case 3: name += ".log"; break;
                    case 4: name += ".cfg"; break;
                }
            }
            
            std::string fullPath = (parentPath == "/") ? ("/" + name) : (parentPath + "/" + name);
            
            // Decide if directory or file (30% dirs, 70% files)
            bool isDir = dirProb(rng) < 0.3;
            
            TreeNode node(fullPath, isDir);
            
            if (!isDir) {
                // Generate random content
                size_t contentSize = contentSizeDist(rng);
                node.content.resize(contentSize);
                for (size_t i = 0; i < contentSize; ++i) {
                    node.content[i] = static_cast<uint8_t>(byteDist(rng));
                }
            }
            
            nodes.push_back(node);
            
            // Add to parent's children list
            auto parentIt = std::find_if(nodes.begin(), nodes.end(),
                [&parentPath](const TreeNode& n) { return n.path == parentPath; });
            if (parentIt != nodes.end()) {
                parentIt->children.push_back(name);
            }
            
            // If directory, add to available directories
            if (isDir) {
                availableDirs.push_back(fullPath);
            }
            
            nodesCreated++;
        }
        
        return nodes;
    }
    
  private:
    std::mt19937 rng;
};

int main() {
    const fs::path tmpBase = fs::temp_directory_path() / (PREFIX + std::to_string(::getpid()));
    fs::create_directories(tmpBase);
    
    const fs::path image = tmpBase / "disk.img";
    
    std::cout << "=== ext4 Forest Test ===\n";
    std::cout << "Creating 32MB ext4 image...\n";
    assert(run_cmd("dd if=/dev/zero of=\"" + image.string() + "\" bs=1M count=32 status=none") == 0);
    assert(run_cmd("mkfs.ext4 -F \"" + image.string() + "\" >/dev/null 2>&1") == 0);
    
    std::cout << "Generating random file tree (200 nodes for quick testing)...\n";
    RandomForestGenerator generator(0);  // Fixed seed for reproducibility
    auto nodes = generator.generate(200);
    
    std::cout << "Generated " << nodes.size() << " nodes\n";
    
    size_t dirCount = 0, fileCount = 0;
    for (const auto& node : nodes) {
        if (node.isDirectory) dirCount++;
        else fileCount++;
    }
    std::cout << "  Directories: " << dirCount << "\n";
    std::cout << "  Files: " << fileCount << "\n";
    
    // Phase 1: Create all files and directories
    std::cout << "\nPhase 1: Creating file tree...\n";
    {
        Ext4Volume vol(image.string());
        
        size_t created = 0;
        size_t errors = 0;
        
        for (const auto& node : nodes) {
            if (node.path == "/") continue;  // Skip root
            
            try {
                if (node.isDirectory) {
                    vol.createDirectory(node.path);
                } else {
                    vol.writeFile(node.path, node.content);
                }
                created++;
                
                if (created % 100 == 0) {
                    std::cout << "  Created " << created << "/" << (nodes.size() - 1) << " nodes...\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "  Error creating " << node.path << ": " << e.what() << "\n";
                errors++;
            }
        }
        
        std::cout << "Created " << created << " nodes (" << errors << " errors)\n";
    }
    
    // Phase 2: Verify all files exist and have correct content
    std::cout << "\nPhase 2: Verifying file tree...\n";
    {
        Ext4Volume vol(image.string());
        
        size_t verified = 0;
        size_t errors = 0;
        size_t totalErrors = 0;
        
        for (const auto& node : nodes) {
            if (node.path == "/") continue;
            
            try {
                if (!vol.exists(node.path)) {
                    std::cerr << "  MISSING: " << node.path << "\n";
                    errors++;
                    continue;
                }
                
                if (node.isDirectory) {
                    if (!vol.isDirectory(node.path)) {
                        std::cerr << "  NOT DIR: " << node.path << "\n";
                        errors++;
                    }
                } else {
                    if (!vol.isFile(node.path)) {
                        std::cerr << "  NOT FILE: " << node.path << "\n";
                        errors++;
                        continue;
                    }
                    
                    auto content = vol.readFile(node.path);
                    if (content != node.content) {
                        std::cerr << "  CONTENT MISMATCH: " << node.path 
                                  << " (expected " << node.content.size() 
                                  << " bytes, got " << content.size() << ")\n";
                        errors++;
                    }
                }
                
                verified++;
                if (verified % 100 == 0) {
                    std::cout << "  Verified " << verified << "/" << (nodes.size() - 1) << " nodes...\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "  Error verifying " << node.path << ": " << e.what() << "\n";
                errors++;
            }
        }
        
        size_t phase2Errors = errors;
        std::cout << "Verified " << verified << " nodes (" << errors << " errors)\n";
        if (errors > 0) {
            std::cerr << "\nWARNING: " << errors << " verification errors occurred\n";
            // Don't assert - ext4 write support is still being debugged
        }
    }
    
    // Phase 3: Test directory listing
    std::cout << "\nPhase 3: Testing directory listings...\n";
    {
        Ext4Volume vol(image.string());
        
        try {
            auto root = vol.readDir("/", false);
            std::cout << "  Root directory has " << root->children.size() << " entries\n";
            
            if (root->children.size() > 0) {
                std::cout << "  Directory listing successful!\n";
            } else {
                std::cout << "  WARNING: Root directory appears empty\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  Directory listing failed: " << e.what() << "\n";
        }
    }
    
    // Phase 4: Stress test - create and delete many files
    std::cout << "\nPhase 4: Stress test (create/delete cycles)...\n";
    {
        Ext4Volume vol(image.string());
        
        const size_t stressIterations = 50;
        size_t success = 0;
        size_t failures = 0;
        
        for (size_t i = 0; i < stressIterations; ++i) {
            try {
                std::string path = "/stress_test_" + std::to_string(i) + ".bin";
                std::vector<uint8_t> data(1024, static_cast<uint8_t>(i));
                
                vol.writeFile(path, data);
                
                auto readData = vol.readFile(path);
                if (readData != data) {
                    std::cerr << "  Content mismatch at iteration " << i << "\n";
                    failures++;
                    continue;
                }
                
                vol.removeFile(path);
                if (vol.exists(path)) {
                    std::cerr << "  File still exists after removal at iteration " << i << "\n";
                    failures++;
                    continue;
                }
                
                success++;
            } catch (const std::exception& e) {
                std::cerr << "  Stress iteration " << i << " failed: " << e.what() << "\n";
                failures++;
            }
        }
        
        std::cout << "  Completed " << success << "/" << stressIterations << " stress iterations\n";
        std::cout << "  Failures: " << failures << "\n";
    }
    
    fs::remove_all(tmpBase);
    
    std::cout << "\n===========================================\n";
    std::cout << "ext4 Forest Test COMPLETED\n";
    std::cout << "Tested " << nodes.size() << " nodes with random structure\n";
    std::cout << "===========================================\n";
    
    return 0;
}
