#include "volume/VolumeManager.hpp"
#include "volume/LocalVolume.hpp"

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

static const char* volumeTypeToString(VolumeType t) {
    switch (t) {
        case VolumeType::HARDDISK: return "HARDDISK";
        case VolumeType::FLOPPY:   return "FLOPPY";
        case VolumeType::CDROM:    return "CDROM";
        case VolumeType::NETWORK:  return "NETWORK";
        case VolumeType::ARCHIVE:  return "ARCHIVE";
        case VolumeType::SYSTEM:   return "SYSTEM";
        case VolumeType::MEMORY:   return "MEMORY";
        case VolumeType::OTHER:    return "OTHER";
        default:                   return "UNKNOWN";
    }
}

int main() {
    VolumeManager vm;
    vm.addLocalVolumes();

    const auto& vols = vm.all();
    for (const auto& uptr : vols) {
        auto* local = dynamic_cast<LocalVolume*>(uptr.get());
        if (!local) {
            continue;
        }

        const auto& mountPointOpt = local->getMountPoint();
        const auto& deviceOpt = local->getDevice();
        auto type = local->getType();

        std::string mountPoint = mountPointOpt ? *mountPointOpt : std::string(local->getRootPath());
        std::string device = deviceOpt ? *deviceOpt : std::string("-");

        std::cout 
            << std::setw(30) << std::left << mountPoint << '\t'
            << std::setw(20) << std::left << device << '\t'
            << volumeTypeToString(type) << '\t'
            << local->getLabel() << '\t'
            << local->getUUID()
            << std::endl;
    }

    return 0;
}

