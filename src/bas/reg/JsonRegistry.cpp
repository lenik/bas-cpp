#include "JsonRegistry.hpp"

#include "strings.hpp"
#include "variant.hpp"

#include "../script/json.hpp"

#include <bas/log/uselog.h>

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace bas::reg {

void JsonRegistry::jsonIn(boost::json::object& o, const JsonFormOptions& opts) { //
    m_doc = o;
}

void JsonRegistry::jsonOut(boost::json::object& obj, const JsonFormOptions& opts) { //
    if (m_doc.is_object()) {
        obj = m_doc.as_object();
    } else {
        // error..
    }
}

std::vector<std::string> JsonRegistry::list(std::string_view path) const {
    auto node = json::locate_const(m_doc, bas::util::replace(path, '.', '/')).node;
    if (node == nullptr)
        return {};
    if (node->is_object()) {
        const auto& obj = node->as_object();
        std::vector<std::string> keys;
        keys.reserve(obj.size());
        for (const auto& [key, value] : obj)
            keys.emplace_back(key);
        return keys;
    }
    if (node->is_array()) {
        std::vector<std::string> keys;
        for (size_t i = 0; i < node->as_array().size(); i++)
            keys.push_back(std::to_string(i));
        return keys;
    }
    return {};
}

std::vector<std::string> JsonRegistry::listDir(std::string_view path) const { //
    return list(path);
}

std::vector<std::string> JsonRegistry::listDomain(std::string_view path) const {
    return list(path);
}

reg::option_t JsonRegistry::getOption(std::string_view path) const {
    auto node = json::locate_const(m_doc, bas::util::replace(path, '.', '/')).node;
    if (node == nullptr)
        return std::nullopt;
    if (node->is_object()) {
        const auto& obj = node->as_object();
        std::string json_string = boost::json::serialize(obj);
        return json_string;
    }
    if (node->is_array()) {
        std::string json_string = boost::json::serialize(node->as_array());
        return json_string;
    }
    return jsonToOption(*node);
}

void JsonRegistry::setOption(std::string_view _path, reg::option_t value) {
    const std::string path = bas::util::replace(_path, '.', '/');
    if (path.empty())
        return;

    auto target = json::locate(m_doc, path, '/', true);
    if (target.node == nullptr)
        return;

    if (value.has_value()) {
        auto jv = optionToJson(value);
        auto old = target.set(jv);
        emitChanged(path, value, jsonToOption(old));
    } else {
        auto old = target.erase();
        emitChanged(path, std::nullopt, jsonToOption(old));
    }
}

bool JsonRegistry::delTree(std::string_view _path) {
    const std::string path = bas::util::replace(_path, '.', '/');
    auto target = json::locate(m_doc, path);
    if (target.is_null())
        return false;

    auto old = target.erase();
    emitChanged(path, std::nullopt, jsonToOption(old));
    return true;
}

} // namespace bas::reg
