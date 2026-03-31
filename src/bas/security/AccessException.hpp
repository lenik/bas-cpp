#ifndef __ACCESSEXCEPTION_H
#define __ACCESSEXCEPTION_H

#include <stdexcept>
#include <string>
#include <string_view>

/**
 * Exception thrown for unauthorized access errors (permission denied, ACL violations, etc.)
 */
class AccessException : public std::runtime_error {
public:
    explicit AccessException(std::string_view message) 
        : std::runtime_error("Access Denied: " + std::string(message)) {}
    
    explicit AccessException(std::string_view operation, std::string_view path, std::string_view details = "")
        : std::runtime_error("Access Denied: " + std::string(operation)
            + " not allowed for '" + std::string(path) + "'" 
            + (details.empty() ? "" : ": " + std::string(details))) {}
};

#endif // __ACCESSEXCEPTION_H
