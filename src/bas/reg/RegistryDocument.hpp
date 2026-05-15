#ifndef OMNISHELL_CORE_REGISTRY_DOCUMENT_HPP
#define OMNISHELL_CORE_REGISTRY_DOCUMENT_HPP

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace os::reg {

boost::json::value parseJsonOrStringBody(std::string_view body);

std::string serializeJsonPretty(const boost::json::value& v);

/** Navigate object keys; missing path -> nullopt. Leaf must be string (registry storage). */
std::optional<std::string> getNestedString(const boost::json::value& root,
                                           const std::vector<std::string>& path);

/** Scalar file: path empty, root becomes JSON string. Object file: create intermediate objects. */
void setNestedString(boost::json::value& root, const std::vector<std::string>& path,
                     const std::string& value);

/** @return true if a leaf was removed. Prunes empty objects. */
bool removeNestedString(boost::json::value& root, const std::vector<std::string>& path);

bool isScalarStringFile(const boost::json::value& root);

} // namespace os::reg

#endif
