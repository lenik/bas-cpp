#include "Volume.hpp"
#include "../fslang/fslang.hpp"
#include "../fslang/fslang_executor.cpp"

#include <sstream>
#include <algorithm>

using namespace fslang;

Volume::ExecutionResult Volume::run(const std::string& fslangSource, 
                                    std::optional<std::function<void()>> commitCallback) {
    // Parse the DSL
    ParseResult parseResult = parse(fslangSource);
    
    if (!parseResult.success) {
        ExecutionResult result;
        result.success = false;
        result.error = "Parse error at line " + std::to_string(parseResult.errorLine) + ": " + parseResult.error;
        result.failedLine = parseResult.errorLine;
        return result;
    }
    
    // Execute the program
    ExecutionResult execResult = execute(parseResult.program, *this, commitCallback);
    
    Volume::ExecutionResult result;
    result.success = execResult.success;
    result.error = execResult.error;
    result.failedLine = execResult.failedLine;
    
    return result;
}

std::string Volume::deepHash() const {
    // Create a mutable copy for traversal (const_cast for traversal only)
    Volume* mutableThis = const_cast<Volume*>(this);
    
    std::function<std::string(const std::string&)> computeHash = 
        [this, &computeHash](const std::string& path) -> std::string {
            std::ostringstream oss;
            
            try {
                if (isFile(path)) {
                    // Hash file content
                    auto data = readFile(path);
                    oss << "F:" << path << ":" << data.size() << ":";
                    for (uint8_t byte : data) {
                        oss << std::hex << (int)byte;
                    }
                } else if (isDirectory(path)) {
                    oss << "D:" << path << ":";
                    
                    // Get directory entries and sort by name
                    auto dir = readDir(path, false);
                    std::vector<std::string> names;
                    for (const auto& [name, child] : dir->children) {
                        names.push_back(name);
                    }
                    std::sort(names.begin(), names.end());
                    
                    // Hash children in sorted order (depth-first)
                    for (const auto& name : names) {
                        std::string childPath = (path == "/") ? ("/" + name) : (path + "/" + name);
                        oss << computeHash(childPath);
                    }
                }
            } catch (...) {
                // Ignore errors in hash computation
            }
            
            return oss.str();
        };
    
    return computeHash("/");
}
