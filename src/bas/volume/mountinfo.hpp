#ifndef MOUNTINFO_HPP
#define MOUNTINFO_HPP

#include <string>

struct MountInfo {
    int mountId;
    int parentId;
    int major;
    int minor;
    std::string root;
    std::string mountPoint;
    std::string vfsOpts;
    std::string fsType;
    std::string device;
    std::string superOpts;
    int dump;
    int pass;

    std::string canonicalMountPoint;

    void reset() {
        mountId = 0;
        parentId = 0;
        major = 0;
        minor = 0;
        root.clear();
        mountPoint.clear();
        vfsOpts.clear();
        fsType.clear();
        device.clear();
        superOpts.clear();
        dump = 0;
        pass = 0;
        canonicalMountPoint.clear();
    }
};

bool parseMountInfoLine(const std::string& line, MountInfo& out);

#endif // MOUNTINFO_HPP
