#ifndef __VOLUMEEXCEPTIONS_H
#define __VOLUMEEXCEPTIONS_H

#include "../io/IOException.hpp"

/**
 * Exception thrown for underlying I/O errors (file system errors, network errors, etc.)
 */
class VolumeException : public IOException {
public:
    explicit VolumeException(std::string_view message) 
        : IOException("Volume Error: " + std::string(message)) {}
    
    explicit VolumeException(std::string_view operation, std::string_view path, std::string_view details = "")
        : IOException("Volume Error: " + std::string(operation)
            + " failed for '" + std::string(path) + "'" 
            + (details.empty() ? "" : ": " + std::string(details))) {}
};

#endif // __VOLUMEEXCEPTIONS_H
