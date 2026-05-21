#ifndef BAS_REG_JSON_REGISTRY_HPP
#define BAS_REG_JSON_REGISTRY_HPP

#include "Registry.hpp"

#include "bas/volume/VolumeFile.hpp"
#include "variant.hpp"

#include "../fmt/JsonForm.hpp"

#include <boost/json.hpp>

#include <filesystem>
#include <functional>
#include <memory>

namespace bas::reg {

/**
 * In-memory JSON tree: '/' and '.' both separate object path segments (see
 * reg::splitUnifiedRegistryPath). Internal leaf keys use canonical '/' form. No file I/O; callers
 * map this document to files if needed.
 */
class JsonRegistry : public IRegistry, public IJsonForm {
  public:
    JsonRegistry() = default;

    JsonRegistry(boost::json::value& doc);

    JsonRegistry(std::function<boost::json::value()> load_fn,
                 std::function<void(boost::json::value&)> save_fn);

    static std::unique_ptr<JsonRegistry> load(const std::filesystem::path& path,
                                              std::string_view encoding = "utf-8");
    static std::unique_ptr<JsonRegistry> load(const VolumeFile& file,
                                              std::string_view encoding = "utf-8");

    virtual ~JsonRegistry() = default;

    void jsonIn(boost::json::object& o, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& obj, const JsonFormOptions& opts) override;

    std::optional<NodeInfo> stat(std::string_view path) const override;

    std::map<regkey_t, NodeInfo> listDir(std::string_view path) const override;
    std::map<regkey_t, NodeInfo> listKeys(std::string_view path) const override;
    std::map<regkey_t, NodeInfo> list(std::string_view path) const override;
    bool delTree(std::string_view path) override;

    reg::option_t getOption(std::string_view path) const override;
    void setOption(std::string_view path, reg::option_t value) override;

    void reset() override;
    void load() override;
    void save() override;

  private:
    boost::json::value m_doc{boost::json::object{}};

    std::function<boost::json::value()> m_load;
    std::function<void(boost::json::value&)> m_save;
};

} // namespace bas::reg

#endif
