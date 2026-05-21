#include "VolumeRegistry.hpp"

#include "strings.hpp"

#include "../script/json.hpp"

#include <bas/log/uselog.h>

#include <boost/json.hpp>

namespace bas::reg {

VolumeRegistry::VolumeRegistry(VolumeFile root, bool autoSave)
    : m_root(root), m_autoSave(autoSave) {}

std::vector<RRL> VolumeRegistry::parseRRL(std::string_view path) const {
    auto rrls = RRL::parse(path, '/');
    for (auto& rrl : rrls) {
        // rrl.container = jsonFilePath(rrl.container);
        rrl.fragment = util::replace(rrl.fragment, '.', '/');
    }
    return rrls;
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
        auto json = vf.readFileUTF8Opt();
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

std::optional<NodeInfo> VolumeRegistry::stat(std::string_view path) const {
    const auto rrls = parseRRL(path);
    NodeInfo info;
    for (std::size_t i = 0; i < rrls.size(); ++i) {
        const RRL& rrl = rrls[i];
        if (!rrl.hasFragment()) {
            auto vdir = m_root.resolve(rrl.container);
            if (vdir && vdir->isDirectory()) {
                info.directory = true;

                DirNode dirNode;
                vdir->readDir(dirNode, false);
                info.fileCount += dirNode.children.size();
            }
        }

        const auto* json = cacheLoadContainer(rrl.container);
        if (json) {
            const auto* node = json;
            if (rrl.hasFragment()) {
                node = json::locate_const(*json, rrl.fragment).node;
                if (node == nullptr)
                    continue;
            }
            if (node->is_object()) {
                info.domain = true;
                info.entryCount += node->as_object().size();
            } else if (node->is_array()) {
                info.domain = true;
                info.entryCount += node->as_array().size();
            } else {
                info.value = true;
            }
        }
    }
    return info.empty() ? std::nullopt : std::optional<NodeInfo>(info);
}

std::map<regkey_t, NodeInfo> VolumeRegistry::list(std::string_view path) const {
    std::map<regkey_t, NodeInfo> map;

    const auto rrls = parseRRL(path);
    for (std::size_t i = 0; i < rrls.size(); ++i) {
        const RRL& rrl = rrls[i];
        if (rrl.hasFragment())
            continue;

        auto dir = m_root.resolve(rrl.container);
        if (!dir || !dir->exists() || !dir->isDirectory())
            continue;

        DirNode dirNode;
        dir->readDir(dirNode, false);

        for (const auto& [name, child] : dirNode.children) {
            if (!child->isDirectory())
                continue;
            map[name] = NodeInfo(true, false);
        }
    }

    auto keyMap = listKeys(path);
    for (const auto& [key, value] : keyMap) {
        bool iterable = value.domain;
        auto old = map.find(key);
        if (old != map.end()) {
            old->second.domain |= iterable;
        } else {
            map[key] = NodeInfo(false, iterable, !iterable);
        }
    }
    return map;
}

std::map<regkey_t, NodeInfo> VolumeRegistry::listDir(std::string_view path) const {
    const auto rrls = parseRRL(path);

    std::map<regkey_t, NodeInfo> map;
    for (std::size_t i = 0; i < rrls.size(); ++i) {
        const RRL& rrl = rrls[i];
        if (rrl.hasFragment())
            continue;

        auto vdir = m_root.resolve(rrl.container);
        if (!vdir || !vdir->exists() || !vdir->isDirectory())
            continue;

        DirNode context;
        vdir->readDir(context, false);

        for (const auto& [name, child] : context.children) {
            bool iterable = (child && child->isDirectory());
            auto old = map.find(name);
            if (old != map.end()) {
                old->second.directory |= iterable;
            } else {
                map[name] = NodeInfo(iterable, false);
            }
        }
    }
    return map;
}

std::map<regkey_t, NodeInfo> VolumeRegistry::listKeys(std::string_view path) const {
    const auto rrls = parseRRL(path);

    std::map<regkey_t, NodeInfo> map;

    for (std::size_t i = 0; i < rrls.size(); ++i) {
        const RRL& rrl = rrls[i];
        const auto* json = cacheLoadContainer(rrl.container);
        if (!json) {
            continue;
        }
        const auto* node = json;
        if (rrl.hasFragment()) {
            node = json::locate_const(*json, rrl.fragment).node;
            if (node == nullptr) {
                continue;
            }
        }
        if (node->is_object()) {
            const auto& obj = node->as_object();
            for (const auto& [_key, value] : obj) {
                auto key = std::string(_key);
                bool iterable = value.is_object() || value.is_array();
                auto old = map.find(key);
                if (old != map.end()) {
                    old->second.domain = iterable;
                } else {
                    map[key] = NodeInfo(false, iterable, !iterable);
                }
            }
        } else if (node->is_array()) {
            for (size_t i = 0; i < node->as_array().size(); i++) {
                auto key = std::to_string(i);
                bool iterable = node->as_array()[i].is_object() || node->as_array()[i].is_array();
                auto old = map.find(key);
                if (old != map.end()) {
                    old->second.domain = iterable;
                } else {
                    map[key] = NodeInfo(false, iterable, !iterable);
                }
            }
        }
    }
    return map;
}

bool VolumeRegistry::delTree(std::string_view path) {
    const auto rrls = parseRRL(path);
    bool dirty = false;

    for (std::size_t i = 0; i < rrls.size(); ++i) {
        const RRL& rrl = rrls[i];
        if (!rrl.hasFragment()) {
            dirty |= cacheRemoveContainer(rrl.container);
            continue;
        }

        auto vf = m_root.resolve(rrl.container);
        if (!vf->isFile()) {
            continue;
        }

        auto* json = cacheLoadContainer(rrl.container);
        if (!json) {
            continue;
        }

        auto target = json::locate_const(*json, rrl.fragment);
        if (target.is_null()) {
            continue;
        }

        auto old = target.erase();
        m_containerDirty[rrl.container] = true;
        dirty |= old.has_value();
    }
    return dirty;
}

reg::option_t VolumeRegistry::getOption(std::string_view path) const {
    const auto rrls = parseRRL(path);
    for (std::size_t i = 0; i < rrls.size(); ++i) {
        const RRL& rrl = rrls[i];
        const auto* jv = cacheLoadFragment(rrl);
        if (jv == nullptr)
            continue;
        else
            return jsonToOption(*jv);
    }
    return std::nullopt;
}

void VolumeRegistry::setOption(std::string_view path, reg::option_t value) {
    const auto rrls = parseRRL(path);
    for (std::size_t i = 0; i < rrls.size(); ++i) {
        const RRL& rrl = rrls[i];
        if (!rrl.hasFragment()) {
            continue;
        }
        boost::json::value jv = optionToJson(value);
        cacheSaveFragment(rrl, jv);
    }
}

void VolumeRegistry::flush() { flushCachedContainers(); }

void VolumeRegistry::save() { flushCachedContainers(); }

} // namespace bas::reg
