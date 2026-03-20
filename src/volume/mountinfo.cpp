#include "mountinfo.hpp"

#include <filesystem>
#include <sstream>
#include <vector>

bool parseMountInfoLine(const std::string& line, MountInfo& out) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
        tokens.push_back(token);

    if (tokens.size() < 10)
        return false;

    size_t dashIdx = 0;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "-") {
            dashIdx = i;
            break;
        }
    }
    if (dashIdx == 0 || dashIdx + 3 >= tokens.size())
        return false;

    // Structure: Each line represents a mount point containing: 
    // 0: mount ID, 
    // 1: parent ID, 
    // 2: major:minor device, 
    // 3: root (path within filesystem), 
    // 4: mount point, 
    // 5: mount options, 
    // 6: - separator between mount options and filesystem type
    // 7: filesystem type, 
    // 8: mount source, 
    // 9: and super options.

    // Example
    // 148 31 259:7 /mmdata /mnt/mmdata rw,noatime shared:112 - ext4 /dev/nvme0n1p7 rw
    // 0: mount ID: 148
    // 1: parent ID: 31
    // 2: major:minor device: 259:7
    // 3: root (path within filesystem): /mmdata
    // 4: mount point: /mnt/mmdata
    // 5: mount options: rw,noatime shared:112
    // ... optional fields...
    // -: separator between mount options and filesystem type
    // +1: filesystem type: ext4
    // +2: mount source: /dev/nvme0n1p7
    // +3: super options: rw

    std::string mountId = tokens[0];
    std::string parentId = tokens[1];
    std::string majMin = tokens[2];
    std::string root = tokens[3];
    std::string mountPoint = tokens[4];
    // std::string vfsOpts = tokens[5];
    std::string fsType = tokens[dashIdx + 1];
    std::string device = tokens[dashIdx + 2];
    // std::string superOpts = tokens[dashIdx + 3];

    out.mountId = std::stoi(mountId);
    out.parentId = std::stoi(parentId);

    int major = 0;
    int minor = 0;
    if (majMin.find(':') != std::string::npos) {
        major = std::stoi(majMin.substr(0, majMin.find(':')));
        minor = std::stoi(majMin.substr(majMin.find(':') + 1));
    } else {
        major = std::stoi(majMin);
    }
    out.major = major;
    out.minor = minor;

    out.root = root;
    out.mountPoint = mountPoint;
    out.fsType = fsType;
    out.device = device;

    std::string vfsJoined;
    for (size_t i = 5; i < dashIdx; ++i) {
        if (!vfsJoined.empty())
            vfsJoined += ',';
        vfsJoined += tokens[i];
    }
    out.vfsOpts = std::move(vfsJoined);

    std::string superJoined;
    for (size_t i = dashIdx + 3; i < tokens.size(); ++i) {
        if (!superJoined.empty())
            superJoined += ',';
        superJoined += tokens[i];
    }
    out.superOpts = std::move(superJoined);

    try {
        out.canonicalMountPoint = std::filesystem::canonical(mountPoint).string();
    } catch (...) {
        out.canonicalMountPoint = std::filesystem::absolute(mountPoint).string();
    }

    return true;
}