#ifndef PATHHELPER_H
#define PATHHELPER_H

#include <string>
#include <vector>

/**
 * Path utility functions - refactored from Qt to standard C++
 */
class PathHelper {
public:
    // Path normalization and manipulation
    static std::string normalizePath(const std::string& path);
    static std::string joinPaths(const std::string& base, const std::string& relative);
    static std::string getParentPath(const std::string& path);
    static std::string getFileName(const std::string& path);
    static std::string getFileExtension(const std::string& path);
    static std::string getBaseName(const std::string& path);
    
    // Path validation
    static bool isAbsolutePath(const std::string& path);
    static bool isValidPath(const std::string& path);
    static bool isSecurePath(const std::string& path, const std::string& basePath);
    
    // Path conversion
    static std::string toUnixPath(const std::string& path);
    static std::string toNativePath(const std::string& path);
    
    // Path components
    static std::vector<std::string> splitPath(const std::string& path);
    static std::string buildPath(const std::vector<std::string>& components);
};

#endif // PATHHELPER_H
