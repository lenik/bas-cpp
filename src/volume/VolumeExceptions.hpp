#ifndef __VOLUMEEXCEPTIONS_H
#define __VOLUMEEXCEPTIONS_H

#include "Volume.hpp"

#include "../io/IOException.hpp"

/**
 * Exception thrown for underlying I/O errors (file system errors, network errors, etc.)
 */
class VolumeException : public IOException {
  public:
    //  Volume Error: <message>
    explicit VolumeException(std::string_view message)
        : IOException("Volume Error: " + std::string(message)) {}

    // Error from volume <source>: <message>
    VolumeException(Volume* vol, std::string_view message)
        : IOException("Error from volume " + vol->getSource() + ": " + std::string(message)) {}

    //  Volume Error: <operation> failed for '<path>': <details>
    VolumeException(std::string_view operation, std::string_view path,
                    std::string_view details = "")
        : IOException("Volume Error: " + std::string(operation)   //
                      + " failed for '" + std::string(path) + "'" //
                      + (details.empty() ? "" : ": " + std::string(details))) {}

    //  Error from volume <source>: <operation> failed for '<path>': <details>
    VolumeException(Volume* vol, std::string_view operation, std::string_view path,
                    std::string_view details = "")
        : IOException("Error from volume " + vol->getSource()     //
                      + ": " + std::string(operation)             //
                      + " failed for '" + std::string(path) + "'" //
                      + (details.empty() ? "" : ": " + std::string(details))) {}
};

#endif // __VOLUMEEXCEPTIONS_H
