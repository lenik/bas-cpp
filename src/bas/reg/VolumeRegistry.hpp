#ifndef OMNISHELL_CORE_VOLUME_REGISTRY_HPP
#define OMNISHELL_CORE_VOLUME_REGISTRY_HPP

#include "Registry.hpp"
#include "Sub.hpp"
#include "variant.hpp"

#include "../volume/VolumeFile.hpp"

#include <string>
#include <vector>

namespace bas::reg {

constexpr std::string_view kDataJson = "DATA.json";

/**
 * Dual-layout registry on a Volume (same key rules as LocalRegistry: '/' = path, '.' = JSON
 * object).
 */
class VolumeRegistry : public IRegistry, public IContainerManager {
  public:
    explicit VolumeRegistry(VolumeFile root, bool autoSave = true);

    std::optional<boost::json::value> readContainer(std::string_view container) const override;
    void writeContainer(std::string_view container, const boost::json::value& value) override;

    std::vector<std::string> list(std::string_view path) const override;
    std::vector<std::string> listDir(std::string_view path) const override;
    std::vector<std::string> listDomain(std::string_view path) const override;
    bool delTree(std::string_view path) override;

    reg::option_t getOption(std::string_view path) const override;
    void setOption(std::string_view path, reg::option_t value) override;

    void flush();
    void save();

  protected:
    RRL parseRRL(std::string_view path) const;

    /** trim(container) + "/DATA.json" at registry root, or DATA.json when container is empty. */
    VolumeFile resolveJsonFile(std::string_view container) const;

    inline VolumeFile resolveJsonFile(RRL rrl) const { return resolveJsonFile(rrl.container); }
    // std::string keyPath(std::string fragment) const;
    // inline std::string keyPath(RRL rrl) const { return keyPath(rrl.fragment); }

    VolumeFile m_root;
    bool m_autoSave{true};

  private:
};

} // namespace bas::reg

#endif
