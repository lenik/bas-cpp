#include "PathHelper.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

std::string PathHelper::normalizePath(const std::string& path) {
    if (path.empty()) {
        return "/";
    }
    
    std::string normalized = path;
    
    // Replace backslashes with forward slashes
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    // Ensure path starts with /
    if (normalized[0] != '/') {
        normalized = "/" + normalized;
    }
    
    // Remove trailing slash unless it's root
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    // Remove double slashes
    size_t pos = 0;
    while ((pos = normalized.find("//", pos)) != std::string::npos) {
        normalized.replace(pos, 2, "/");
    }
    
    return normalized;
}

std::string PathHelper::joinPaths(const std::string& base, const std::string& relative) {
    if (base.empty()) return normalizePath(relative);
    if (relative.empty()) return normalizePath(base);
    
    std::string result = base;
    if (result.back() != '/') {
        result += '/';
    }
    
    std::string rel = relative;
    if (rel.front() == '/') {
        rel = rel.substr(1);
    }
    
    result += rel;
    return normalizePath(result);
}

std::string PathHelper::getParentPath(const std::string& path) {
    std::string normalized = normalizePath(path);
    
    if (normalized == "/") {
        return "/"; // Root has no parent
    }
    
    size_t lastSlash = normalized.find_last_of('/');
    if (lastSlash == 0) {
        return "/"; // Parent is root
    }
    
    return normalized.substr(0, lastSlash);
}

std::string PathHelper::getFileName(const std::string& path) {
    std::string normalized = normalizePath(path);
    
    if (normalized == "/") {
        return "";
    }
    
    size_t lastSlash = normalized.find_last_of('/');
    if (lastSlash == std::string::npos) {
        return normalized;
    }
    
    return normalized.substr(lastSlash + 1);
}

std::string PathHelper::getFileExtension(const std::string& path) {
    std::string fileName = getFileName(path);
    
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos == std::string::npos || dotPos == 0) {
        return "";
    }
    
    return fileName.substr(dotPos + 1);
}

std::string PathHelper::getBaseName(const std::string& path) {
    std::string fileName = getFileName(path);
    
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos == std::string::npos || dotPos == 0) {
        return fileName;
    }
    
    return fileName.substr(0, dotPos);
}

bool PathHelper::isAbsolutePath(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

bool PathHelper::isValidPath(const std::string& path) {
    if (path.empty()) return false;
    
    // Check for invalid characters
    const std::string invalidChars = "\0";
    for (char c : invalidChars) {
        if (path.find(c) != std::string::npos) {
            return false;
        }
    }
    
    // Check for path traversal attempts
    if (path.find("..") != std::string::npos) {
        return false;
    }
    
    return true;
}

bool PathHelper::isSecurePath(const std::string& path, const std::string& basePath) {
    if (!isValidPath(path) || !isValidPath(basePath)) {
        return false;
    }
    
    try {
        fs::path pathObj = fs::weakly_canonical(path);
        fs::path baseObj = fs::weakly_canonical(basePath);
        
        // Check if path is within basePath
        auto pathStr = pathObj.string();
        auto baseStr = baseObj.string();
        
        return pathStr.substr(0, baseStr.length()) == baseStr;
    } catch (const fs::filesystem_error&) {
        return false;
    }
}

std::string PathHelper::toUnixPath(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

std::string PathHelper::toNativePath(const std::string& path) {
#ifdef _WIN32
    std::string result = path;
    std::replace(result.begin(), result.end(), '/', '\\');
    return result;
#else
    return path;
#endif
}

std::vector<std::string> PathHelper::splitPath(const std::string& path) {
    std::vector<std::string> components;
    std::string normalized = normalizePath(path);
    
    if (normalized == "/") {
        return components; // Empty for root
    }
    
    std::stringstream ss(normalized);
    std::string component;
    
    while (std::getline(ss, component, '/')) {
        if (!component.empty()) {
            components.push_back(component);
        }
    }
    
    return components;
}

std::string PathHelper::buildPath(const std::vector<std::string>& components) {
    if (components.empty()) {
        return "/";
    }
    
    std::string result = "/";
    for (size_t i = 0; i < components.size(); ++i) {
        if (i > 0) {
            result += "/";
        }
        result += components[i];
    }
    
    return result;
}
