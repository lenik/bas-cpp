#include "VolumeRegistry.hpp"

#include "strings.hpp"

#include "../script/json.hpp"

#include <bas/log/uselog.h>

#include <boost/json.hpp>

#include <algorithm>

namespace bas::reg {

VolumeRegistry::VolumeRegistry(VolumeFile root, bool autoSave)
    : m_root(root), m_autoSave(autoSave) {}

RRL VolumeRegistry::parseRRL(std::string_view path) const {
    auto rrl = RRL::_parse(path, '/');
    // rrl.container = jsonFilePath(rrl.container);
    rrl.fragment = util::replace(rrl.fragment, '.', '/');
    return rrl;
}

VolumeFile VolumeRegistry::resolveJsonFile(std::string_view container) const {
    const std::string dir = util::trimTrailingSlash(container);
    std::string path = dir;
    if (dir.empty())
        path = kDataJson;
    else
        path = dir + "/" + std::string(kDataJson);
    return *m_root.resolve(path);
}

std::optional<boost::json::value> VolumeRegistry::readContainer(std::string_view container) const {
    auto vf = resolveJsonFile(container);
    if (!vf.exists() || !vf.isFile()) {
        return std::nullopt;
    }

    try {
        auto json = vf.readFileUTF8();
        if (!json.has_value()) {
            return std::nullopt;
        }
        return boost::json::parse(*json);
    } catch (const std::exception& e) {
        logerror_fmt("Failed to parse JSON file %s: %s", vf.getPath().c_str(), e.what());
        return std::nullopt;
    }
}

void VolumeRegistry::writeContainer(std::string_view container, const boost::json::value& value) {
    auto vf = resolveJsonFile(container);
    if (vf.isDirectory()) {
        logerror_fmt("Failed to write JSON file %s: is a directory", vf.getPath().c_str());
        return;
    }

    vf.createParentDirectories();

    auto json = boost::json::serialize(value);
    vf.writeFileString(json);
}

std::vector<std::string> VolumeRegistry::list(std::string_view path) const {
    auto dirs = listDir(path);
    auto domains = listDomain(path);
    dirs.reserve(dirs.size() + domains.size());
    for (const auto& key : domains) {
        if (std::find(dirs.begin(), dirs.end(), key) == dirs.end()) {
            dirs.push_back(key);
        }
    }
    return dirs;
}

std::vector<std::string> VolumeRegistry::listDir(std::string_view path) const {
    const auto rrl = parseRRL(path);
    if (!rrl.fragment.empty()) {
        return {};
    }
    auto vdir = m_root.resolve(rrl.container);
    if (!vdir || !vdir->exists() || !vdir->isDirectory()) {
        return {};
    }
    DirNode context;
    vdir->readDir(context, false);
    std::vector<std::string> result;
    for (const auto& [name, child] : context.children) {
        if (child && child->isDirectory()) {
            result.push_back(name);
        }
    }
    return result;
}

std::vector<std::string> VolumeRegistry::listDomain(std::string_view path) const {
    const auto rrl = parseRRL(path);
    auto* json = cacheLoadContainerForQuery(rrl.container);
    if (!json) {
        return {};
    }
    const boost::json::value* node = json;
    if (!rrl.fragment.empty()) {
        node = json::locate_const(*json, rrl.fragment).node;
        if (node == nullptr) {
            return {};
        }
    }
    if (node->is_object()) {
        const auto& obj = node->as_object();
        std::vector<std::string> keys;
        for (const auto& [key, value] : obj) {
            keys.emplace_back(key);
        }
        return keys;
    }
    if (node->is_array()) {
        std::vector<std::string> keys;
        for (size_t i = 0; i < node->as_array().size(); i++) {
            keys.emplace_back(std::to_string(i));
        }
        return keys;
    }
    return {};
}

bool VolumeRegistry::delTree(std::string_view path) {
    const auto rrl = parseRRL(path);
    if (rrl.fragment.empty()) {
        return cacheRemoveContainer(rrl.container);
    }
    auto vf = m_root.resolve(rrl.container);
    if (!vf->isFile()) {
        return false;
    }
    auto* json = cacheLoadContainer(rrl.container);
    if (!json) {
        return false;
    }
    auto target = json::locate_const(*json, rrl.fragment);
    if (target.is_null()) {
        return false;
    }
    auto old = target.erase();
    m_containerDirty[rrl.container] = true;
    return old.has_value();
}

reg::option_t VolumeRegistry::getOption(std::string_view path) const {
    const auto rrl = parseRRL(path);
    const auto* jv = cacheLoadFragment(rrl);
    if (jv == nullptr) {
        return std::nullopt;
    }
    return jsonToOption(*jv);
}

void VolumeRegistry::setOption(std::string_view path, reg::option_t value) {
    const auto rrl = parseRRL(path);
    boost::json::value jv = optionToJson(value);
    cacheSaveFragment(rrl, jv);
}

} // namespace bas::reg
