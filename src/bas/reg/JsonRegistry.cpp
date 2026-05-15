#include "JsonRegistry.hpp"

#include "RegistryDocument.hpp"
#include "RegistryKeyUtil.hpp"
#include "RegistryPath.hpp"
#include "RegistryValueIo.hpp"

#include <algorithm>
#include <functional>

namespace os {

std::string JsonRegistry::normalizeKey(const std::string& key) {
    return reg::canonicalUnifiedRegistryKey(key);
}

void JsonRegistry::ensureLoaded() const {
    if (m_loaded)
        return;
    const_cast<JsonRegistry*>(this)->load();
}

void JsonRegistry::rebuildFlatKeys() {
    m_data.clear();
    if (!m_doc.is_object())
        return;
    std::function<void(const boost::json::object&, const std::string&)> walk;
    walk = [&](const boost::json::object& o, const std::string& prefix) {
        for (const auto& it : o) {
            const std::string k(it.key());
            std::string next = prefix.empty() ? k : prefix + "/" + k;
            if (it.value().is_object()) {
                walk(it.value().as_object(), next);
            } else if (it.value().is_string()) {
                m_data[next] = std::string(it.value().as_string().c_str());
            }
        }
    };
    walk(m_doc.as_object(), "");
}

bool JsonRegistry::load() {
    m_loaded = false;
    m_data.clear();
    m_doc = boost::json::object{};
    m_loaded = true;
    return true;
}

bool JsonRegistry::save() const {
    return true;
}

std::vector<std::string> JsonRegistry::list(const std::string& node_key, bool full_key) const {
    ensureLoaded();
    const std::string nk = node_key.empty() ? node_key : normalizeKey(node_key);
    return reg::listChildKeys(m_data, nk, full_key);
}

reg::variant_t JsonRegistry::getVariant(const std::string& key) const {
    ensureLoaded();
    const std::string nk = normalizeKey(key);
    if (nk.empty())
        return std::nullopt;
    auto it = m_data.find(nk);
    if (it == m_data.end())
        return std::nullopt;
    return reg::parseValue(it->second);
}

void JsonRegistry::setVariant(const std::string& key, reg::variant_t value) {
    ensureLoaded();
    const std::string nk = normalizeKey(key);
    if (nk.empty())
        return;
    auto segs = reg::splitUnifiedRegistryPath(key);
    if (segs.empty())
        return;

    reg::variant_t old;
    if (auto it = m_data.find(nk); it != m_data.end())
        old = reg::parseValue(it->second);

    if (!value.has_value()) {
        if (reg::removeNestedString(m_doc, segs)) {
            rebuildFlatKeys();
            emitChanged(nk, std::nullopt, old);
        }
        return;
    }
    reg::setNestedString(m_doc, segs, reg::valueToString(*value));
    rebuildFlatKeys();
    emitChanged(nk, value, old);
}

bool JsonRegistry::has(const std::string& key) const {
    ensureLoaded();
    const std::string nk = normalizeKey(key);
    if (nk.empty())
        return false;
    return m_data.find(nk) != m_data.end();
}

bool JsonRegistry::remove(const std::string& key) {
    ensureLoaded();
    const std::string nk = normalizeKey(key);
    if (nk.empty())
        return false;
    auto segs = reg::splitUnifiedRegistryPath(key);
    if (segs.empty())
        return false;
    auto it = m_data.find(nk);
    if (it == m_data.end())
        return false;
    reg::variant_t old = reg::parseValue(it->second);
    if (!reg::removeNestedString(m_doc, segs))
        return false;
    rebuildFlatKeys();
    emitChanged(nk, std::nullopt, old);
    return true;
}

bool JsonRegistry::delTree(const std::string& key) {
    ensureLoaded();
    const std::string nk = normalizeKey(key);
    if (nk.empty())
        return false;
    std::vector<std::string> keys = reg::keysForDelTree(m_data, nk);
    if (keys.empty())
        return false;
    std::sort(keys.begin(), keys.end(), [](const std::string& a, const std::string& b) {
        return reg::splitUnifiedRegistryPath(a).size() > reg::splitUnifiedRegistryPath(b).size();
    });
    for (const auto& k : keys) {
        auto segs = reg::splitUnifiedRegistryPath(k);
        if (segs.empty())
            continue;
        auto it = m_data.find(k);
        reg::variant_t old = it != m_data.end() ? reg::parseValue(it->second) : std::nullopt;
        if (reg::removeNestedString(m_doc, segs))
            emitChanged(k, std::nullopt, old);
    }
    rebuildFlatKeys();
    return true;
}

std::map<std::string, std::string> JsonRegistry::snapshotStrings() const {
    ensureLoaded();
    return m_data;
}

} // namespace os
