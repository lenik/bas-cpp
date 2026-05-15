#ifndef OMNISHELL_CORE_VOLUME_REGISTRY_HPP
#define OMNISHELL_CORE_VOLUME_REGISTRY_HPP

#include "AbstractRegistry.hpp"
#include "RegistryPath.hpp"

#include <bas/volume/VolumeFile.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace os {

/**
 * Dual-layout registry on a Volume (same key rules as LocalRegistry: '/' = path, '.' = JSON object).
 */
class VolumeRegistry : public AbstractRegistry {
  public:
    explicit VolumeRegistry(VolumeFile root);

    bool load() override;
    bool save() const override;

    std::vector<std::string> list(const std::string& node_key, bool full_key) const override;

    reg::variant_t getVariant(const std::string& key) const override;
    void setVariant(const std::string& key, reg::variant_t value) override;

    bool has(const std::string& key) const override;
    bool remove(const std::string& key) override;
    bool delTree(const std::string& key) override;

    std::map<std::string, std::string> snapshotStrings() const override;

  protected:
    void ensureLoaded() const;

    void loadFromVolume();
    bool writeAllToVolume() const;
    std::unique_ptr<VolumeFile> fileForDual(const reg::DualPathResolution& r) const;

    void syncDualFileForGroup(const reg::DualPathResolution& sample);
    void collectKeysForGroup(const reg::DualPathResolution& sample,
                             std::map<std::vector<std::string>, std::string>& out) const;

  private:
    VolumeFile m_root;
};

} // namespace os

#endif
