#ifndef OMNISHELL_CORE_LOCAL_REGISTRY_HPP
#define OMNISHELL_CORE_LOCAL_REGISTRY_HPP

#include "VolumeRegistry.hpp"

#include <bas/volume/LocalVolume.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace os {

/**
 * Host registry: same dual-path rules as VolumeRegistry ( '/' = path to .json, '.' = JSON fields ).
 *
 * Singleton \ref instance uses ~/.config/<wxApp>/registry on a LocalVolume over the user's home dir.
 *
 * \ref LocalRegistry(path) anchors the Volume at that filesystem directory (the registry tree root).
 * \ref forApp(name) builds `~/.config/<name>/registry`.
 */
class LocalRegistry : public VolumeRegistry {
  public:
    explicit LocalRegistry(std::filesystem::path registry_root);

    static LocalRegistry forApp(std::string_view app_name);

    static LocalRegistry& instance();

    bool load() override;

  private:
    struct VolPathInit {
        std::shared_ptr<LocalVolume> volume;
        std::filesystem::path abs_dir;
    };

    explicit LocalRegistry(VolPathInit init);

    LocalRegistry();

    static VolPathInit makeVolPathInit(std::filesystem::path registry_root);

    bool parseLegacyFlatJsonFile(const std::string& path);

    std::string getLegacyRegistryJsonPath() const;

    std::filesystem::path m_registryDirAbs;
    std::shared_ptr<LocalVolume> m_volumeHolder;
};

} // namespace os

#endif
