# FSLang Test Suite

## Overview

This directory contains FSLang (Filesystem Language) test scripts for comprehensive filesystem testing. FSLang provides a declarative DSL for describing filesystem operations and verifying results.

## Test Files

### FAT32 Tests

1. **fat32_basic.fsl** - Core functionality test
   - Directory creation and navigation
   - File creation (UTF-8 and binary)
   - Copy, move, rename operations
   - Delete operations
   - Directory listing
   - Hash computation

2. **fat32_stress.fsl** - Stress test
   - Creates 100+ files
   - Deep directory nesting (5 levels)
   - Special characters in filenames
   - Larger content files
   - Hash verification

3. **Fat32_fslang_test.cpp** - C++ test runner
   - Loads FSLang scripts
   - Executes against in-memory FAT32 image
   - Reports pass/fail status
   - Tracks commit count

### ext4 Tests

1. **ext4_basic.fsl** - Core functionality test
   - Same operations as fat32_basic.fsl
   - Validates ext4-specific behavior
   - Computes ext4 filesystem hash

2. **Ext4_fslang_test.cpp** - C++ test runner
   - Similar to FAT32 runner
   - Uses ext4 image format

## Running Tests

### Build
```bash
cd /home/udisk/bas-cpp
ninja -C build
```

### Run Individual Tests
```bash
# FAT32 FSLang tests
./build/Fat32_fslang_test

# ext4 FSLang tests  
./build/Ext4_fslang_test
```

### Run All Tests
```bash
ninja -C build test
```

## FSLang Syntax

### Commands

```
# Comments start with #

# Directory operations
mkdir "path"              # Create directory
rmdir "path"              # Remove directory
cd "path"                 # Change directory

# File operations
put "file" "content"      # Create file (UTF-8)
put "file" h"hexdata"     # Create file (binary)
copy "src" "dst"          # Copy file
move "src" "dst"          # Move file
rename "path" "newname"   # Rename file
delete "path"             # Delete file or directory

# Verification
check "file" "content"    # Verify content (UTF-8)
check "file" h"hexdata"   # Verify content (binary)
ls "path"                 # List directory
hash ["var"]              # Compute hash

# Control
assert "condition"        # Assertion
print "message"           # Print message
```

### Example Script

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

# Cleanup
delete "/test/subdir"

# Hash
hash "final_hash"
print "Hash: ${final_hash}"
```

## Test Coverage

### Operations Tested

✅ Directory creation (mkdir)
✅ Directory removal (rmdir)  
✅ Directory navigation (cd)
✅ File creation - UTF-8 (put)
✅ File creation - Binary (put h"")
✅ File copy (copy)
✅ File move (move)
✅ File rename (rename)
✅ File delete (delete)
✅ Content verification (check)
✅ Directory listing (ls)
✅ Hash computation (hash)
✅ Assertions (assert)
✅ Output (print)

### Edge Cases Tested

- Deep directory nesting
- Special characters in filenames
- Empty files
- Binary content
- Large files
- Multiple operations sequence
- Hash consistency

## Commit Callback

Each side-effect operation (mkdir, put, copy, move, rename, delete) triggers a commit callback. This enables:

- Snapshotting filesystem state
- Logging operations
- Transaction support
- Undo/redo functionality

Read-only operations (cd, check, ls, hash, assert, print) do NOT trigger commits.

## Hash Computation

The `deepHash()` function computes a consistent hash over the entire filesystem:

- Names sorted alphabetically
- Depth-first traversal
- Includes file content and structure
- Hash remains stable unless content changes
- Useful for regression testing

## Adding New Tests

1. Create `.fsl` script in `tests/fslang/`
2. Add test runner if needed
3. Update `meson.build` to include new test
4. Run `ninja -C build` to build
5. Execute test and verify results

## Debugging

When a test fails:
1. Check error message for line number
2. Review FSLang script at that line
3. Verify filesystem state with `ls` commands
4. Use `print` for debugging
5. Check hash values for consistency

## Performance

- Tests run in-memory (MemDevice)
- No disk I/O overhead
- Fast execution (<1 second per test)
- Suitable for CI/CD pipelines

## Future Enhancements

- [ ] Variable substitution
- [ ] Conditional execution (if/else)
- [ ] Loops (for each directory entry)
- [ ] Pattern matching in paths
- [ ] Include directive for modular tests
- [ ] Test fixtures and setup/teardown
- [ ] Parallel test execution
- [ ] Coverage reporting
