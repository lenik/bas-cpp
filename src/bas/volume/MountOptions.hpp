#ifndef MOUNTOPTIONS_H
#define MOUNTOPTIONS_H

#include <string>
#include <memory>

/**
 * Filesystem mount options.
 */
 struct MountOptions {
    
    /** Mount read-only */
    bool readOnly = false;
    
    /** Character encoding for filenames (default UTF-8) */
    std::string encoding = "UTF-8";

    /** Default UID for created files */
    uint32_t uid = 0;
    
    /** Default GID for created files */
    uint32_t gid = 0;
    
    /** Default file permissions */
    uint16_t fileMode = 0644;
    
    /** Default directory permissions */
    uint16_t dirMode = 0755;
  
    /** File permissions mask */
    uint16_t fileMask = 0777;
    
    /** Directory permissions mask */
    uint16_t dirMask = 0777;

};

#endif // MOUNTOPTIONS_H
