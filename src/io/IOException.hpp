#ifndef IOEXCEPTION_H
#define IOEXCEPTION_H

#include <stdexcept>
#include <string>
#include <string_view>

/**
 * Exception thrown for underlying I/O errors (file system errors, network errors, etc.)
 */
class IOException : public std::runtime_error {
public:
    explicit IOException(std::string_view message) 
        : std::runtime_error("IO Error: " + std::string(message)) {}
    
    explicit IOException(std::string_view operation, std::string_view path, std::string_view details = "")
        : std::runtime_error("IO Error: " + std::string(operation) 
        + " failed for '" + std::string(path) + "'" 
        + (details.empty() ? "" : ": " + std::string(details))) {}

};

#endif // IOEXCEPTION_H
