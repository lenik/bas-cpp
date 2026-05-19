#ifndef BAS_REG_JSON_REGISTRY_HPP
#define BAS_REG_JSON_REGISTRY_HPP

#include "Registry.hpp"

#include "variant.hpp"

#include "../fmt/JsonForm.hpp"

#include <boost/json.hpp>

#include <string>

namespace bas::reg {

/**
 * In-memory JSON tree: '/' and '.' both separate object path segments (see
 * reg::splitUnifiedRegistryPath). Internal leaf keys use canonical '/' form. No file I/O; callers
 * map this document to files if needed.
 */
class JsonRegistry : public IRegistry, public IJsonForm {
  public:
    JsonRegistry() = default;

    void jsonIn(boost::json::object& o, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& obj, const JsonFormOptions& opts) override;

    std::vector<std::string> listDir(std::string_view path) const override;
    std::vector<std::string> listDomain(std::string_view path) const override;
    std::vector<std::string> list(std::string_view path) const override;
    bool delTree(std::string_view path) override;

    reg::option_t getOption(std::string_view path) const override;
    void setOption(std::string_view path, reg::option_t value) override;

  private:
    boost::json::value m_doc;
};

} // namespace bas::reg

#endif
