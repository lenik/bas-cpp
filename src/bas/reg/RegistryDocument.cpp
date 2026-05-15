#include "RegistryDocument.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

namespace os::reg {

boost::json::value parseJsonOrStringBody(std::string_view body) {
    boost::system::error_code ec;
    boost::json::value v = boost::json::parse(boost::json::string_view(body.data(), body.size()), ec);
    if (!ec)
        return v;
    return boost::json::value{std::string(body)};
}

std::string serializeJsonPretty(const boost::json::value& v) {
    return boost::json::serialize(v);
}

bool isScalarStringFile(const boost::json::value& root) {
    return root.is_string();
}

std::optional<std::string> getNestedString(const boost::json::value& root,
                                             const std::vector<std::string>& path) {
    if (path.empty()) {
        if (root.is_string())
            return std::string(root.as_string().c_str());
        return std::nullopt;
    }
    if (!root.is_object())
        return std::nullopt;
    const boost::json::object* cur = &root.as_object();
    for (size_t i = 0; i < path.size(); ++i) {
        auto it = cur->find(path[i]);
        if (it == cur->end())
            return std::nullopt;
        if (i + 1 == path.size()) {
            if (!it->value().is_string())
                return std::nullopt;
            return std::string(it->value().as_string().c_str());
        }
        if (!it->value().is_object())
            return std::nullopt;
        cur = &it->value().as_object();
    }
    return std::nullopt;
}

void setNestedString(boost::json::value& root, const std::vector<std::string>& path,
                     const std::string& value) {
    if (path.empty()) {
        root = boost::json::value(value);
        return;
    }
    if (!root.is_object()) {
        root = boost::json::object{};
    }
    boost::json::object* cur = &root.as_object();
    for (size_t i = 0; i < path.size(); ++i) {
        const std::string& key = path[i];
        if (i + 1 == path.size()) {
            (*cur)[key] = boost::json::value(value);
            return;
        }
        auto it = cur->find(key);
        if (it == cur->end() || !it->value().is_object()) {
            (*cur)[key] = boost::json::object{};
        }
        cur = &(*cur)[key].as_object();
    }
}

bool removeNestedString(boost::json::value& root, const std::vector<std::string>& path) {
    if (path.empty()) {
        if (root.is_string()) {
            root = boost::json::object{};
            return true;
        }
        return false;
    }
    if (!root.is_object())
        return false;

    struct Frame {
        boost::json::object* obj;
        std::string key;
    };
    std::vector<Frame> trail;
    boost::json::object* cur = &root.as_object();

    for (size_t i = 0; i < path.size(); ++i) {
        const std::string& key = path[i];
        auto it = cur->find(key);
        if (it == cur->end())
            return false;
        if (i + 1 == path.size()) {
            if (!it->value().is_string())
                return false;
            cur->erase(it);
            for (int j = static_cast<int>(trail.size()) - 1; j >= 0; --j) {
                boost::json::object* parent = trail[static_cast<size_t>(j)].obj;
                const std::string& pk = trail[static_cast<size_t>(j)].key;
                auto pit = parent->find(pk);
                if (pit != parent->end() && pit->value().is_object() &&
                    pit->value().as_object().empty()) {
                    parent->erase(pit);
                } else {
                    break;
                }
            }
            return true;
        }
        if (!it->value().is_object())
            return false;
        trail.push_back({cur, key});
        cur = &it->value().as_object();
    }
    return false;
}

} // namespace os::reg
