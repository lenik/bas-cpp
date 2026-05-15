#include "VolumeRegistry.hpp"

#include "RegistryDocument.hpp"
#include "RegistryKeyUtil.hpp"
#include "RegistryValueIo.hpp"

#include <wx/log.h>

#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <functional>

namespace os {

namespace {

std::string dualToRelPath(const reg::DualPathResolution& r) {
    std::string rel;
    for (const auto& d : r.dirSegments) {
        if (!rel.empty())
            rel += '/';
        rel += d;
    }
    if (!rel.empty())
        rel += '/';
    rel += r.fileStem + ".json";
    return rel;
}

bool endsWithJson(std::string_view n) {
    return n.size() >= 5 && n.compare(n.size() - 5, 5, ".json") == 0;
}

} // namespace

VolumeRegistry::VolumeRegistry(VolumeFile root) : m_root(std::move(root)) {}

void VolumeRegistry::ensureLoaded() const {
    if (m_loaded)
        return;
    const_cast<VolumeRegistry*>(this)->loadFromVolume();
    const_cast<VolumeRegistry*>(this)->m_loaded = true;
}

void VolumeRegistry::collectKeysForGroup(
    const reg::DualPathResolution& sample,
    std::map<std::vector<std::string>, std::string>& out) const {
    out.clear();
    for (const auto& kv : m_data) {
        reg::DualPathResolution r;
        if (!reg::resolveDualPath(kv.first, r))
            continue;
        if (r.dirSegments != sample.dirSegments || r.fileStem != sample.fileStem)
            continue;
        out[r.objectPath] = kv.second;
    }
}

std::unique_ptr<VolumeFile> VolumeRegistry::fileForDual(const reg::DualPathResolution& r) const {
    std::string rel = dualToRelPath(r);
    if (rel.empty())
        return nullptr;
    return m_root.resolve(rel);
}

void VolumeRegistry::syncDualFileForGroup(const reg::DualPathResolution& sample) {
    std::map<std::vector<std::string>, std::string> leaves;
    collectKeysForGroup(sample, leaves);

    auto f = fileForDual(sample);
    if (!f) {
        wxLogWarning("VolumeRegistry: cannot resolve path for sync");
        return;
    }

    if (leaves.empty()) {
        f->removeFile();
        return;
    }

    bool hasNested = false;
    for (const auto& e : leaves) {
        if (!e.first.empty()) {
            hasNested = true;
            break;
        }
    }

    boost::json::value doc;
    if (!hasNested && leaves.size() == 1 && leaves.begin()->first.empty()) {
        doc = boost::json::value(leaves.begin()->second);
    } else {
        doc = boost::json::object{};
        for (const auto& e : leaves) {
            if (!e.first.empty())
                reg::setNestedString(doc, e.first, e.second);
        }
    }

    if (!f->createParentDirectories()) {
        wxLogWarning("VolumeRegistry: createParentDirectories failed");
        return;
    }
    std::string body = reg::serializeJsonPretty(doc);
    try {
        f->writeFileString(body, "UTF-8");
    } catch (...) {
        wxLogWarning("VolumeRegistry: write failed");
    }
}

void VolumeRegistry::loadFromVolume() {
    m_data.clear();

    std::function<void(const VolumeFile&, const std::string&)> walk;
    walk = [&](const VolumeFile& vDir, const std::string& slashPrefix) {
        auto dir = vDir.readDir(false);
        for (const auto& [name, child] : dir->children) {
            if (!child)
                continue;
            if (child->isDirectory()) {
                auto sub = vDir.resolve(child->name);
                if (sub) {
                    std::string next = slashPrefix.empty()
                                           ? std::string(child->name)
                                           : slashPrefix + "/" + std::string(child->name);
                    walk(*sub, next);
                }
            } else if (endsWithJson(child->name)) {
                std::string name = child->name;
                std::string stem = name.substr(0, name.size() - 5);
                std::string dualBase = slashPrefix.empty() ? stem : slashPrefix + "/" + stem;
                auto vf = vDir.resolve(child->name);
                if (vf && vf->isFile()) {
                    try {
                        std::string body = vf->readFileString("UTF-8");
                        boost::json::value rootVal = reg::parseJsonOrStringBody(body);
                        if (reg::isScalarStringFile(rootVal)) {
                            m_data[dualBase] = std::string(rootVal.as_string().c_str());
                        } else if (rootVal.is_object()) {
                            std::function<void(const boost::json::object&, const std::string&)>
                                flatten;
                            flatten = [&](const boost::json::object& o,
                                          const std::string& dotPath) {
                                for (const auto& it : o) {
                                    const std::string k(it.key());
                                    std::string nextPath = dotPath.empty() ? k : dotPath + "." + k;
                                    if (it.value().is_object()) {
                                        flatten(it.value().as_object(), nextPath);
                                    } else if (it.value().is_string()) {
                                        m_data[dualBase + "." + nextPath] =
                                            std::string(it.value().as_string().c_str());
                                    }
                                }
                            };
                            flatten(rootVal.as_object(), "");
                        }
                    } catch (...) {
                    }
                }
            }
        }
    };

    try {
        walk(m_root, "");
    } catch (...) {
        wxLogWarning("VolumeRegistry: load walk failed");
    }

    if (m_data.empty()) {
        m_data["System/OS/Name"] = "Omnishell";
        m_data["System/OS/Version"] = "1.1.1";
        m_data["User/Name"] = "Guest";
        m_data["User/Role"] = "Guest";
        if (!writeAllToVolume())
            wxLogWarning("VolumeRegistry: seed write failed");
        else
            wxLogInfo("VolumeRegistry: seeded defaults (%zu entries)", m_data.size());
    } else {
        wxLogInfo("VolumeRegistry: loaded %zu entries from volume", m_data.size());
    }
}

bool VolumeRegistry::writeAllToVolume() const {
    std::map<std::pair<std::vector<std::string>, std::string>, bool> seen;
    for (const auto& kv : m_data) {
        reg::DualPathResolution r;
        if (!reg::resolveDualPath(kv.first, r))
            continue;
        auto gk = std::make_pair(r.dirSegments, r.fileStem);
        if (seen.count(gk))
            continue;
        seen[gk] = true;
        const_cast<VolumeRegistry*>(this)->syncDualFileForGroup(r);
    }
    return true;
}

bool VolumeRegistry::load() {
    m_loaded = false;
    m_data.clear();
    loadFromVolume();
    m_loaded = true;
    return true;
}

bool VolumeRegistry::save() const {
    ensureLoaded();
    return writeAllToVolume();
}

std::vector<std::string> VolumeRegistry::list(const std::string& node_key, bool full_key) const {
    ensureLoaded();
    return reg::listChildKeys(m_data, reg::normalizeDualLookupKey(node_key), full_key);
}

reg::variant_t VolumeRegistry::getVariant(const std::string& key) const {
    ensureLoaded();
    auto it = m_data.find(reg::normalizeDualLookupKey(key));
    if (it == m_data.end())
        return std::nullopt;
    return reg::parseValue(it->second);
}

void VolumeRegistry::setVariant(const std::string& key, reg::variant_t value) {
    ensureLoaded();
    const std::string nk = reg::normalizeDualLookupKey(key);
    reg::DualPathResolution r;
    if (!reg::resolveDualPath(nk, r))
        return;

    reg::variant_t old;
    if (auto it = m_data.find(nk); it != m_data.end())
        old = reg::parseValue(it->second);

    if (!value.has_value()) {
        if (m_data.erase(nk)) {
            syncDualFileForGroup(r);
            emitChanged(nk, std::nullopt, old);
        }
        return;
    }
    m_data[nk] = reg::valueToString(*value);
    syncDualFileForGroup(r);
    emitChanged(nk, value, old);
}

bool VolumeRegistry::has(const std::string& key) const {
    ensureLoaded();
    return m_data.find(reg::normalizeDualLookupKey(key)) != m_data.end();
}

bool VolumeRegistry::remove(const std::string& key) {
    ensureLoaded();
    const std::string nk = reg::normalizeDualLookupKey(key);
    reg::DualPathResolution r;
    if (!reg::resolveDualPath(nk, r))
        return false;
    auto it = m_data.find(nk);
    if (it == m_data.end())
        return false;
    reg::variant_t oldv = reg::parseValue(it->second);
    m_data.erase(it);
    syncDualFileForGroup(r);
    emitChanged(nk, std::nullopt, oldv);
    return true;
}

bool VolumeRegistry::delTree(const std::string& key) {
    ensureLoaded();
    std::vector<std::string> keys = reg::keysForDelTree(m_data, reg::normalizeDualLookupKey(key));
    if (keys.empty())
        return false;
    std::vector<reg::DualPathResolution> groups;
    for (const auto& k : keys) {
        reg::DualPathResolution r;
        if (reg::resolveDualPath(k, r))
            groups.push_back(std::move(r));
    }
    for (const auto& k : keys) {
        auto it = m_data.find(k);
        if (it == m_data.end())
            continue;
        reg::variant_t oldv = reg::parseValue(it->second);
        m_data.erase(it);
        emitChanged(k, std::nullopt, oldv);
    }
    std::sort(groups.begin(), groups.end(),
              [](const reg::DualPathResolution& a, const reg::DualPathResolution& b) {
                  if (a.dirSegments != b.dirSegments)
                      return a.dirSegments < b.dirSegments;
                  return a.fileStem < b.fileStem;
              });
    groups.erase(
        std::unique(groups.begin(), groups.end(),
                    [](const reg::DualPathResolution& a, const reg::DualPathResolution& b) {
                        return a.dirSegments == b.dirSegments && a.fileStem == b.fileStem;
                    }),
        groups.end());
    for (const auto& r : groups)
        syncDualFileForGroup(r);
    return true;
}

std::map<std::string, std::string> VolumeRegistry::snapshotStrings() const {
    ensureLoaded();
    return m_data;
}

} // namespace os
