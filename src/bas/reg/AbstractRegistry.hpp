#ifndef OMNISHELL_CORE_ABSTRACT_REGISTRY_HPP
#define OMNISHELL_CORE_ABSTRACT_REGISTRY_HPP

#include "IRegistry.hpp"

#include <map>
#include <string>
#include <string_view>

namespace os {

/**
 * Shared state and JSON string helpers for registry backends (Dual, Json tree, Fs, etc.).
 */
class AbstractRegistry : public IRegistry {
  public:
    ~AbstractRegistry() override = default;

  protected:
    mutable bool m_loaded{false};
    std::map<std::string, std::string> m_data;

    static std::string escapeJsonString(const std::string& in);
    static std::string parseJsonStringLiteral(std::string_view s);
};

} // namespace os

#endif
