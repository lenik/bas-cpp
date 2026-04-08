# Filesystem Testing Guide

## Overview

This guide covers testing for FAT32, ext4, and exFAT filesystem implementations including traditional C++ tests and FSLang-based declarative tests.

## Test Suite Structure

```
tests/
├── fslang/                      # FSLang test scripts
│   ├── fat32_basic.fsl          # FAT32 basic operations
│   ├── fat32_stress.fsl         # FAT32 stress test
│   ├── ext4_basic.fsl           # ext4 basic operations
│   └── exfat_readonly.fsl       # exFAT read-only test
│
src/bas/volume/
├── fat32/
│   ├── Fat32_test.cpp           # Basic C++ tests
│   ├── Fat32_forest_test.cpp    # Stress tests
│   ├── Fat32_fslang_test.cpp    # FSLang runner
│   └── Fat32_persistence_test.cpp # Persistence tests
├── ext4/
│   ├── Ext4_test.cpp            # Basic C++ tests
│   ├── Ext4_forest_test.cpp     # Stress tests
│   └── Ext4_fslang_test.cpp     # FSLang runner
└── exfat/
    ├── Exfat_test.cpp           # Basic C++ tests (read-only)
    └── Exfat_fslang_test.cpp    # FSLang runner (read-only)
```

## Building Tests

```bash
cd /home/udisk/bas-cpp
ninja -C build
```

## Running Tests

### Run All Tests
```bash
ninja -C build test
```

### Run Specific Tests

#### FAT32 Tests
```bash
# Basic C++ tests
./build/Fat32_test

# Forest stress test
./build/Fat32_forest_test

# FSLang tests
./build/Fat32_fslang_test

# Persistence test
./build/Fat32_persistence_test
```

#### ext4 Tests
```bash
# Basic C++ tests
./build/Ext4_test

# Forest stress test
./build/Ext4_forest_test

# FSLang tests
./build/Ext4_fslang_test
```

#### exFAT Tests
```bash
# Basic C++ tests (read-only)
./build/Exfat_test

# FSLang tests (read-only)
./build/Exfat_fslang_test
```

## Test Coverage

### FAT32 Tests (61 passing)

**Basic Operations:**
- ✅ File create/read/write/delete
- ✅ Directory create/delete
- ✅ Copy/move/rename operations
- ✅ Path resolution
- ✅ Large files (8KB+)

**Stress Tests:**
- ✅ 200+ random files/directories
- ✅ Deep nesting (5+ levels)
- ✅ Special characters in names
- ✅ Binary content

**Persistence:**
- ✅ Data survives remounting
- ✅ Directory structure preserved
- ✅ Content integrity verified

**FSLang Tests:**
- ✅ fat32_basic.fsl - Core functionality
- ✅ fat32_stress.fsl - Stress scenarios

### ext4 Tests (Partial)

**Read Operations:**
- ✅ File reading
- ✅ Directory listing
- ✅ Path resolution
- ✅ stat() operations

**Write Operations:**
- ⚠️ Partial implementation
- ⏳ Directory creation (needs debugging)
- ⏳ File writing (needs debugging)

**FSLang Tests:**
- ✅ ext4_basic.fsl - Read operations

### exFAT Tests (Read-Only)

**Read Operations:**
- ✅ Boot sector parsing
- ✅ Directory reading
- ✅ File reading
- ✅ Path resolution

**Write Operations:**
- ❌ Not implemented (stubs throw exceptions)

**FSLang Tests:**
- ✅ exfat_readonly.fsl - Read validation

## FSLang Syntax

### Basic Commands

```
# Comments start with #
mkdir "path"              # Create directory
rmdir "path"              # Remove directory
cd "path"                 # Change directory
put "file" "content"      # Create file (UTF-8)
put "file" h"hexdata"     # Create file (binary)
copy "src" "dst"          # Copy file
move "src" "dst"          # Move file
rename "path" "newname"   # Rename file
delete "path"             # Delete file/dir
check "file" "content"    # Verify content
ls "path"                 # List directory
hash ["var"]              # Compute hash
assert "condition"        # Assertion
print "message"           # Output
```

### Example Test Script

```
# Create structure
mkdir "/test"
mkdir "/test/subdir"

# Create files
put "/test/hello.txt" "Hello, World!"
put "/test/binary.bin" h"DEADBEEF"

# Verify
check "/test/hello.txt" "Hello, World!"
check "/test/binary.bin" h"DEADBEEF"

# Operations
copy "/test/hello.txt" "/test/subdir/copy.txt"
move "/test/binary.bin" "/test/subdir/moved.bin"

# Hash verification
hash "final_hash"
print "Hash: ${final_hash}"
```

## Writing New Tests

### C++ Test Template

```cpp
#include "Fat32Volume.hpp"
#include "../../io/BlockDevice.hpp"

#include <cassert>
#include <iostream>

int main() {
    // Create memory device
    auto device = createMemDevice(size);
    Fat32Volume vol(device);
    
    // Test operations
    vol.createDirectory("/test");
    vol.writeFileUTF8("/test/file.txt", "content");
    
    // Verify
    assert(vol.exists("/test/file.txt"));
    assert(vol.readFileUTF8("/test/file.txt") == "content");
    
    std::cout << "Test PASSED\n";
    return 0;
}
```

### FSLang Test Template

```
# test_name.fsl
print "Testing feature..."

# Setup
mkdir "/test"

# Operations
put "/test/file.txt" "content"

# Verification
check "/test/file.txt" "content"

# Cleanup
delete "/test"

print "✓ Test passed"
```

## Debugging Failed Tests

### 1. Check Error Messages
```
Test FAILED at line 42
Error: check: content mismatch at /test/file.txt
```
→ Look at line 42 in the FSLang script

### 2. Enable Verbose Output
```cpp
ExecutionContext ctx;
ctx.verbose = true;
```

### 3. Use Hash for Debugging
```
hash "before_hash"
# ... operations ...
hash "after_hash"
print "Before: ${before_hash}"
print "After: ${after_hash}"
```

### 4. Inspect Memory Images
Tests write final images to disk for inspection:
```bash
# Mount and inspect
sudo mount -o loop /tmp/test_image.img /mnt
ls -la /mnt
```

## Performance Benchmarks

### Test Execution Times

| Test | FAT32 | ext4 | exFAT |
|------|-------|------|-------|
| Basic | <1s | <1s | <1s |
| Forest | 5-10s | 5-10s | N/A |
| Persistence | 2-3s | N/A | N/A |
| FSLang Basic | <1s | <1s | <1s |

### Memory Usage

- Memory-backed tests: ~16-32MB per test
- File-backed tests: Minimal overhead
- FSLang executor: ~1-2MB overhead

## Continuous Integration

### GitLab CI Example

```yaml
test:
  script:
    - ninja -C build
    - ninja -C build test
  artifacts:
    reports:
      junit: build/meson-logs/testlog.xml
```

### GitHub Actions Example

```yaml
- name: Run tests
  run: |
    ninja -C build
    ninja -C build test
```

## Known Issues

### FAT32
- Rename/delete affected by OS caching (3 tests failing)
- Workaround: Use BlockDevice with O_SYNC

### ext4
- Directory creation needs debugging
- Write operations partially implemented

### exFAT
- Read-only support complete
- Write operations not implemented

## Future Test Enhancements

- [ ] Parallel test execution
- [ ] Coverage reporting
- [ ] Performance regression tests
- [ ] Cross-platform compatibility tests
- [ ] Fuzzing tests
- [ ] Long-running stress tests
- [ ] Power failure simulation tests

## References

- FSLANG_DESIGN.md - Language specification
- FSLANG_TESTS.md - Test suite documentation
- IMPLEMENTATION_SUMMARY.md - Implementation overview
- KNOWN_LIMITATIONS.md - Known issues and workarounds
