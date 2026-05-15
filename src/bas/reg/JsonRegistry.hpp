#ifndef OMNISHELL_CORE_JSON_REGISTRY_HPP
#define OMNISHELL_CORE_JSON_REGISTRY_HPP

#include "AbstractRegistry.hpp"

#include <boost/json.hpp>

#include <string>

namespace os {

/**
 * In-memory JSON tree: '/' and '.' both separate object path segments (see reg::splitUnifiedRegistryPath).
 * Internal leaf keys use canonical '/' form. No file I/O; callers map this document to files if needed.
 */
class JsonRegistry : public AbstractRegistry {
  public:
    JsonRegistry() = default;

    bool load() override;
    bool save() const override;

    std::vector<std::string> list(const std::string& node_key, bool full_key) const override;

    reg::variant_t getVariant(const std::string& key) const override;
    void setVariant(const std::string& key, reg::variant_t value) override;

    bool has(const std::string& key) const override;
    bool remove(const std::string& key) override;
    bool delTree(const std::string& key) override;

    std::map<std::string, std::string> snapshotStrings() const override;

    const boost::json::value& document() const { return m_doc; }
    boost::json::value& document() { return m_doc; }

  private:
    void ensureLoaded() const;
    void rebuildFlatKeys();
    static std::string normalizeKey(const std::string& key);

    boost::json::value m_doc{boost::json::object{}};
};

} // namespace os

#endif
