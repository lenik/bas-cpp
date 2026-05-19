#include "LocalRegistry.hpp"

#include "../volume/LocalVolume.hpp"
#include "../volume/VolumeFile.hpp"

namespace bas::reg {

LocalRegistry::LocalRegistry(std::string_view root_dir, bool autoSave)
    : VolumeRegistry(VolumeFile(&LocalVolume::home(), std::string(root_dir)), autoSave) {
    if (auto* vol = m_root.getVolume()) {
        const std::string& rootPath = m_root.getPath();
        if (!rootPath.empty() && !vol->exists(rootPath)) {
            vol->createDirectories(rootPath);
        }
    }
}

LocalRegistry LocalRegistry::s_user_default(".config/registry");

LocalRegistry& LocalRegistry::userDefault() { return s_user_default; }

} // namespace bas::reg
