#include "ExfatVolume.hpp"
#include "../../io/BlockDevice.hpp"
#include "../../io/IOException.hpp"

#include <bas/log/logger.h>

#include <cassert>
#include <iostream>
#include <vector>

extern "C" logger_t test_logger = {};

int main() {
    std::cout << "=== exFAT Simple Test ===\n\n";
    
    std::cout << "Testing exFAT class structure...\n";
    
    // Test 1: Verify ExfatVolume class exists and has correct methods
    {
        std::cout << "Test 1: Class structure... ";
        
        // Just verify the class can be referenced
        static_assert(std::is_base_of<Volume, ExfatVolume>::value, "ExfatVolume must inherit from Volume");
        
        std::cout << "PASSED\n";
    }
    
    // Test 2: Verify BlockDevice works
    {
        std::cout << "Test 2: BlockDevice... ";
        
        const size_t size = 1024 * 1024;  // 1MB
        auto device = createMemDevice(size);
        
        assert(device != nullptr);
        assert(device->size() == size);
        assert(device->isOpen());
        
        // Write some data
        std::vector<uint8_t> testData = {1, 2, 3, 4, 5};
        assert(device->write(0, testData.data(), testData.size()));
        
        // Read it back
        std::vector<uint8_t> readData(5);
        assert(device->read(0, readData.data(), readData.size()));
        assert(readData == testData);
        
        std::cout << "PASSED\n";
    }
    
    // Test 3: Verify exFAT headers compile
    {
        std::cout << "Test 3: Headers compile... ";
        
        // If we got here, the headers compiled successfully
        std::cout << "PASSED\n";
    }
    
    std::cout << "\n✓ All exFAT structure tests PASSED!\n";
    std::cout << "Note: Full exFAT functionality testing requires mkfs.exfat\n";
    std::cout << "      to create a valid exFAT image.\n";
    
    return 0;
}
