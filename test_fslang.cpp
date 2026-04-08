#include "src/bas/volume/Volume.hpp"
#include "src/bas/volume/fat32/Fat32Volume.hpp"
#include "src/bas/io/BlockDevice.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

using namespace std;

int main() {
    cout << "=== FSLang Demo ===\n\n";
    
    // Create a FAT32 image in memory
    const size_t imageSize = 16 * 1024 * 1024;  // 16MB
    auto device = createMemDevice(imageSize);
    
    // Format it (simplified - in real usage would use mkfs.fat)
    cout << "Creating filesystem...\n";
    
    // Create volume
    Fat32Volume vol(device);
    
    // Define FSLang script
    string script = R"(
# FSLang Test Script
# Comments are ignored

# Create directory structure
mkdir "/test"
mkdir "/test/subdir"

# Create files with UTF-8 content
put "/test/hello.txt" "Hello, World!"
put "/test/subdir/data.txt" "Nested file content"

# Create files with hex content
put "/test/binary.bin" h"DEADBEEF"

# Navigate
cd "/test"
put "local.txt" "Created in test dir"
cd "/"

# Copy and move
copy "/test/hello.txt" "/test/hello_copy.txt"
move "/test/local.txt" "/test/subdir/moved.txt"

# Verify content
check "/test/hello.txt" "Hello, World!"
check "/test/binary.bin" h"DEADBEEF"

# List directory
ls "/test"

# Compute hash
hash "fs_hash"
print "Filesystem hash: ${fs_hash}"

# Cleanup
delete "/test/hello_copy.txt"
)";
    
    cout << "Executing FSLang script...\n\n";
    
    int commitCount = 0;
    auto commitCallback = [&commitCount]() {
        commitCount++;
        cout << "  [Commit #" << commitCount << "]\n";
    };
    
    // Execute the script
    auto result = vol.run(script, commitCallback);
    
    if (result.success) {
        cout << "\n✓ FSLang script executed successfully!\n";
        cout << "  Total commits: " << commitCount << "\n";
        
        // Compute and display final hash
        string hash = vol.deepHash();
        cout << "  Final filesystem hash: " << hash.substr(0, 64) << "...\n";
    } else {
        cout << "\n✗ FSLang script failed at line " << result.failedLine << "\n";
        cout << "  Error: " << result.error << "\n";
        return 1;
    }
    
    cout << "\n=== Demo Complete ===\n";
    return 0;
}
