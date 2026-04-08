# FSLang - Filesystem Testing DSL

## Overview

FSLang is a domain-specific language for declarative filesystem testing. It provides a clean, readable syntax for creating, manipulating, and verifying filesystem structures.

## Grammar

```
# Comments start with #
# Blank lines are ignored

mkdir "path"                    # Create directory
rmdir "path"                    # Remove directory  
cd "path"                       # Change working directory
put "file" "content"            # Create file with UTF-8 content
put "file" h"aabbccdd"          # Create file with hex content
copy "src" "dst"                # Copy file
move "src" "dst"                # Move/rename file
rename "path/base" "newname"    # Rename file (keep same parent)
check "file" "content"          # Verify file content (UTF-8)
check "file" h"aabbccdd"        # Verify file content (hex)
delete "path"                   # Delete file or directory
ls "path"                       # List directory
hash ["var"]                    # Compute filesystem hash
assert "condition"              # Simple assertion
print "message"                 # Print message
```

## Implementation

### Files Created

1. **src/fslang/fslang.hpp** - AST definitions and parser interface
2. **src/fslang/fslang.lex** - Flex lexer specification  
3. **src/fslang/fslang.y** - Bison parser grammar
4. **src/fslang/fslang_executor.cpp** - Command executor
5. **src/fslang/meson.build** - Build configuration
6. **src/bas/volume/Volume_fslang.cpp** - Volume integration

### Volume Integration

```cpp
// Run FSLang script
Volume::ExecutionResult run(
    const std::string& fslangSource,
    std::optional<std::function<void()>> commitCallback = std::nullopt
);

// Compute consistent filesystem hash
std::string deepHash() const;
```

### Features

**Commit Callback:**
- Called after every side-effect command (mkdir, put, delete, etc.)
- Not called for readonly commands (cd, check, ls, hash)
- Enables snapshotting or logging between operations

**Deep Hash:**
- Computes hash over entire filesystem structure
- Consistent ordering: names sorted, depth-first traversal
- Hash remains stable unless filesystem content changes
- Useful for verifying filesystem state in tests

## Example Usage

```cpp
#include "bas/volume/Volume.hpp"
#include "bas/volume/fat32/Fat32Volume.hpp"

Fat32Volume vol("/path/to/image.img");

std::string script = R"(
    mkdir "/test"
    put "/test/hello.txt" "Hello, World!"
    put "/test/binary.bin" h"DEADBEEF"
    check "/test/hello.txt" "Hello, World!"
    check "/test/binary.bin" h"DEADBEEF"
    hash
)";

auto result = vol.run(script, []() {
    std::cout << "Changes committed\n";
});

if (result.success) {
    std::cout << "Test passed!\n";
} else {
    std::cout << "Test failed at line " << result.failedLine 
              << ": " << result.error << "\n";
}
```

## Benefits

1. **Declarative** - Describe WHAT to test, not HOW
2. **Readable** - Clear, self-documenting test scripts
3. **Maintainable** - Easy to modify test scenarios
4. **Reproducible** - Same script always produces same result
5. **Verifiable** - Hash ensures filesystem state consistency

## Status

✅ Parser grammar defined  
✅ Lexer specification complete  
✅ AST data structures defined  
✅ Executor framework implemented  
✅ Volume integration methods added  
✅ Deep hash algorithm implemented  
⏳ Bison/Flex integration (requires build)  
⏳ Full command implementation  
⏳ Comprehensive test suite  

## Next Steps

1. Complete Bison/Flex build integration
2. Implement all command executors
3. Add variable substitution support
4. Add conditional execution (if/else)
5. Add loops (for each entry in directory)
6. Create comprehensive test suite
7. Add error recovery and reporting
8. Document full language specification
