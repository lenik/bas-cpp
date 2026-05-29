#include "JsonRegistry.hpp"

#include "strings.hpp"
#include "variant.hpp"

#include "../script/json.hpp"

#include <bas/log/uselog.h>
#include <bas/util/unicode.hpp>

#include <boost/json.hpp>

#include <fstream>
#include <optional>
#include <string>

namespace bas::reg {

JsonRegistry::JsonRegistry(std::function<boost::json::value()> load_fn,
                           std::function<void(boost::json::value&)> save_fn)
    : m_load(std::move(load_fn)), m_save(std::move(save_fn)) {
    JsonRegistry::load();
}

JsonRegistry::JsonRegistry(boost::json::value& doc)
    : m_doc(doc), m_load([&]() { return doc; }),
      m_save([&](boost::json::value& doc) { doc = m_doc; }) {
    if (!doc.is_object()) {
        throw std::runtime_error("JSON registry root must be an object: " +
                                 boost::json::serialize(doc));
    }
}

std::unique_ptr<JsonRegistry> JsonRegistry::load(const std::filesystem::path& path,
                                                 std::string_view encoding) {
    auto load = [path, encoding]() {
        std::ifstream in(path);
        if (!in) {
            throw std::runtime_error("failed to open " + path.string());
        }
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
        if (bytes.empty()) {
            return boost::json::value(boost::json::object{});
        }
        auto utf8 = unicode::toUtf8String(bytes, encoding);
        auto doc = boost::json::parse(utf8);
        if (!doc.is_object()) {
            throw std::runtime_error("JSON registry file root must be an object: " + path.string());
        }
        return doc;
    };

    auto save = [path](boost::json::value& doc) {
        std::ofstream out(path);
        if (!out) {
            throw std::runtime_error("failed to write " + path.string());
        }
        out << boost::json::serialize(doc);
    };
    return std::make_unique<JsonRegistry>(std::move(load), std::move(save));
}

std::unique_ptr<JsonRegistry> JsonRegistry::load(const VolumeFile& file,
                                                 std::string_view encoding) {
    auto load = [file, encoding]() {
        if (!file.exists() || !file.isFile()) {
            return boost::json::value(boost::json::object{});
        }
        auto bytes = file.readFile();
        if (bytes.empty()) {
            return boost::json::value(boost::json::object{});
        }
        auto utf8 = unicode::toUtf8String(bytes, encoding);
        auto doc = boost::json::parse(utf8);
        if (!doc.is_object()) {
            throw std::runtime_error("JSON registry file root must be an object: " +
                                     file.getPath());
        }
        return doc;
    };

    auto save = [file, encoding](boost::json::value& doc) {
        auto utf8 = boost::json::serialize(doc);
        file.writeFileString(utf8, encoding);
    };
    return std::make_unique<JsonRegistry>(std::move(load), std::move(save));
}

void JsonRegistry::jsonIn(const boost::json::object& o, const JsonFormOptions& opts) { //
    m_doc = o;
}

void JsonRegistry::jsonOut(boost::json::object& obj, const JsonFormOptions& opts) { //
    if (m_doc.is_object()) {
        obj = m_doc.as_object();
    } else {
        // error..
    }
}

std::optional<NodeInfo> JsonRegistry::stat(std::string_view path) const {
    auto node = json::locate_const(m_doc, bas::util::replace(path, '.', '/')).node;
    if (node == nullptr)
        return std::nullopt;
    bool iterable = node->is_object() || node->is_array();
    return NodeInfo(iterable, iterable);
}

std::map<regkey_t, NodeInfo> JsonRegistry::list(std::string_view path) const {
    auto node = json::locate_const(m_doc, bas::util::replace(path, '.', '/')).node;
    if (node == nullptr)
        return {};

    std::map<regkey_t, NodeInfo> map;
    if (node->is_object()) {
        const auto& obj = node->as_object();
        for (const auto& [key, value] : obj) {
            bool iterable = value.is_object() || value.is_array();
            map.emplace(std::string(key), NodeInfo(iterable, iterable, !iterable));
        }
    } else if (node->is_array()) {
        auto& array = node->as_array();
        for (size_t i = 0; i < array.size(); i++) {
            bool iterable = array[i].is_object() || array[i].is_array();
            map.emplace(i, NodeInfo(iterable, iterable, !iterable));
        }
    }
    return map;
}

std::map<regkey_t, NodeInfo> JsonRegistry::listDir(std::string_view path) const {
    auto map = list(path);
    return map;
}

std::map<regkey_t, NodeInfo> JsonRegistry::listKeys(std::string_view path) const {
    auto map = list(path);
    return map;
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

void JsonRegistry::reset() { m_doc = boost::json::object{}; }

void JsonRegistry::load() { m_doc = m_load(); }

void JsonRegistry::save() { m_save(m_doc); }

} // namespace bas::reg
