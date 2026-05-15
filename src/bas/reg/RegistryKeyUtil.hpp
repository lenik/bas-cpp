#ifndef OMNISHELL_CORE_REGISTRY_KEY_UTIL_HPP
#define OMNISHELL_CORE_REGISTRY_KEY_UTIL_HPP

#include <map>
#include <string>
#include <vector>

namespace os::reg {

/**
 * Registry keys use '/' for filesystem-style segments and '.' for object path inside the leaf JSON file
 * (Dual / Json-tree layouts). Children under node_key are the next path segment after '/' or '.'.
 */
std::vector<std::string> listChildKeys(const std::map<std::string, std::string>& data,
                                       const std::string& node_key, bool full_key);

/** Remove node_key and descendants (prefix with '/' or '.' delimiter, not plain substring). */
std::vector<std::string> keysForDelTree(const std::map<std::string, std::string>& data,
                                        const std::string& node_key);

} // namespace os::reg

#endif
