#include "LocalRegistry.hpp"

#include "RegistryPath.hpp"

#include <bas/proc/env.hpp>
#include <bas/volume/LocalVolume.hpp>

#include <wx/app.h>
#include <wx/log.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace fs = std::filesystem;

namespace {

bool isDirEmpty(const fs::path& dir) {
    if (!fs::exists(dir))
        return true;
    std::error_code ec;
    auto it = fs::directory_iterator(dir, ec);
    if (ec)
        return true;
    return it == fs::directory_iterator{};
}

std::unique_ptr<LocalVolume>& localHomeVolume() {
    static std::unique_ptr<LocalVolume> v;
    return v;
}

fs::path defaultSingletonRegistryDirAbs() {
    fs::path home = fs::path(bas::getHomePath());
    std::string app = "omnishell";
    if (wxTheApp)
        app = wxTheApp->GetAppName().ToStdString();
    return home / ".config" / app / "registry";
}

VolumeFile makeSingletonVolumeRoot(const fs::path& registry_dir_abs) {
    auto& vol = localHomeVolume();
    if (!vol)
        vol = std::make_unique<LocalVolume>(bas::getHomePath());
    fs::path home = fs::path(bas::getHomePath());
    std::error_code ec;
    fs::path rel = fs::relative(registry_dir_abs, home, ec);
    std::string volPath;
    if (!ec && !rel.empty()) {
        volPath = rel.generic_string();
        if (!volPath.empty() && volPath[0] != '/')
            volPath.insert(volPath.begin(), '/');
    } else {
        std::string homeStr = home.generic_string();
        std::string dirStr = registry_dir_abs.generic_string();
        if (!homeStr.empty() && dirStr.size() > homeStr.size() && dirStr.compare(0, homeStr.size(), homeStr) == 0) {
            volPath = dirStr.substr(homeStr.size());
            if (!volPath.empty() && volPath[0] != '/')
                volPath.insert(volPath.begin(), '/');
        } else {
            volPath = "/";
        }
    }
    return VolumeFile(vol.get(), volPath);
}

struct SingletonRegistryRoots {
    fs::path dir_abs;
    VolumeFile volume_root;

    SingletonRegistryRoots()
        : dir_abs(defaultSingletonRegistryDirAbs()),
          volume_root(makeSingletonVolumeRoot(dir_abs)) {}
};

SingletonRegistryRoots& singletonRegistryRoots() {
    static SingletonRegistryRoots s;
    return s;
}

} // namespace

namespace os {

LocalRegistry::VolPathInit LocalRegistry::makeVolPathInit(std::filesystem::path registry_root) {
    VolPathInit out;
    std::error_code ec;
    fs::path abs = fs::weakly_canonical(fs::absolute(registry_root), ec);
    if (ec)
        abs = fs::absolute(registry_root);
    out.abs_dir = abs;
    out.volume = std::make_shared<LocalVolume>(abs.string());
    return out;
}

LocalRegistry::LocalRegistry(VolPathInit init)
    : VolumeRegistry(VolumeFile(init.volume.get(), "/")),
      m_registryDirAbs(std::move(init.abs_dir)),
      m_volumeHolder(std::move(init.volume)) {}

LocalRegistry::LocalRegistry(std::filesystem::path registry_root)
    : LocalRegistry(makeVolPathInit(std::move(registry_root))) {}

LocalRegistry LocalRegistry::forApp(std::string_view app_name) {
    fs::path base = fs::path(bas::getHomePath());
    return LocalRegistry(base / ".config" / std::string(app_name) / "registry");
}

LocalRegistry::LocalRegistry()
    : VolumeRegistry(singletonRegistryRoots().volume_root),
      m_registryDirAbs(singletonRegistryRoots().dir_abs),
      m_volumeHolder() {}

LocalRegistry& LocalRegistry::instance() {
    static LocalRegistry inst;
    return inst;
}

std::string LocalRegistry::getLegacyRegistryJsonPath() const {
    return (m_registryDirAbs.parent_path() / "registry.json").string();
}

bool LocalRegistry::parseLegacyFlatJsonFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open())
        return false;

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string json = buffer.str();

    size_t pos = 0;
    while (true) {
        size_t keyPos = json.find('"', pos);
        if (keyPos == std::string::npos)
            break;
        size_t keyEnd = json.find('"', keyPos + 1);
        if (keyEnd == std::string::npos)
            break;
        std::string key = json.substr(keyPos + 1, keyEnd - keyPos - 1);

        size_t colon = json.find(':', keyEnd);
        if (colon == std::string::npos)
            break;
        size_t valStart = json.find('"', colon);
        if (valStart == std::string::npos)
            break;
        size_t valEnd = json.find('"', valStart + 1);
        if (valEnd == std::string::npos)
            break;
        std::string value = json.substr(valStart + 1, valEnd - valStart - 1);

        m_data[os::reg::migrateLegacyFlatKey(key)] = value;
        pos = valEnd + 1;
    }

    return !m_data.empty();
}

bool LocalRegistry::load() {
    m_loaded = false;
    m_data.clear();

    std::string legacyPath = getLegacyRegistryJsonPath();

    if (fs::exists(legacyPath) && isDirEmpty(m_registryDirAbs)) {
        if (parseLegacyFlatJsonFile(legacyPath)) {
            std::error_code ec;
            fs::create_directories(m_registryDirAbs, ec);
            m_loaded = true;
            if (writeAllToVolume()) {
                try {
                    fs::rename(fs::path(legacyPath), fs::path(legacyPath + ".bak"));
                } catch (...) {
                }
                wxLogInfo("LocalRegistry: migrated legacy registry.json (%zu entries)", m_data.size());
                return true;
            }
            m_data.clear();
            wxLogWarning("LocalRegistry: migration write failed; falling back to scan/seed");
        }
    }

    return VolumeRegistry::load();
}

} // namespace os
