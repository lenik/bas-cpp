#include "json.hpp"

#include <cassert>
#include <charconv>

namespace boost::json {

bool get_bool(const boost::json::object& json, const char* key, bool default_value) {
    if (!key)
        throw std::invalid_argument("null key");
    if (json.if_contains(key))
        return json.at(key).as_bool();
    else
        return default_value;
}

int get_int(const boost::json::object& json, const char* key, int default_value) {
    if (!key)
        throw std::invalid_argument("null key");
    if (json.if_contains(key)) {
        int64_t val = json.at(key).as_int64();
        return val;
    } else
        return default_value;
}

unsigned int get_uint(const boost::json::object& json, const char* key,
                      unsigned int default_value) {
    if (!key)
        throw std::invalid_argument("null key");
    if (json.if_contains(key)) {
        uint64_t val = json.at(key).as_uint64();
        return val;
    } else
        return default_value;
}

int64_t get_int64(const boost::json::object& json, const char* key, int64_t default_value) {
    if (!key)
        throw std::invalid_argument("null key");
    if (json.if_contains(key))
        return json.at(key).as_int64();
    else
        return default_value;
}

uint64_t get_uint64(const boost::json::object& json, const char* key, uint64_t default_value) {
    if (!key)
        throw std::invalid_argument("null key");
    if (json.if_contains(key))
        return json.at(key).as_uint64();
    else
        return default_value;
}

double get_double(const boost::json::object& json, const char* key, double default_value) {
    if (!key)
        throw std::invalid_argument("null key");
    if (json.if_contains(key))
        return json.at(key).as_double();
    else
        return default_value;
}

std::string get_string(const boost::json::object& json, const char* key,
                       const char* default_value) {
    if (!key)
        throw std::invalid_argument("null key");
    if (!default_value)
        throw std::invalid_argument("null default_value");
    if (json.if_contains(key)) {
        const boost::json::string& js = json.at(key).as_string();
        return std::string(js.begin(), js.end());
    } else {
        return default_value ? default_value : "";
    }
}

} // namespace boost::json

namespace bas::json {

std::string json_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string json_unescape(std::string_view s) {
    while (!s.empty() &&
           (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' || s.front() == '\r'))
        s.remove_prefix(1);
    while (!s.empty() &&
           (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r'))
        s.remove_suffix(1);
    if (s.empty())
        return "";
    if (s.front() != '"')
        return std::string(s);

    std::string out;
    for (size_t i = 1; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"')
            break;
        if (c == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case '\\':
                out += '\\';
                break;
            case '"':
                out += '"';
                break;
            default:
                out += s[i];
                break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

// Helper to handle RFC 6901 tokenization and unescaping (~1 -> /, ~0 -> ~)
std::vector<std::string> tokenize(std::string_view path, char separator) {
    std::vector<std::string> tokens;
    if (path.empty() || (path.size() == 1 && path[0] == separator))
        return tokens;

    // Discard leading separator if present
    if (path[0] == separator) {
        path.remove_prefix(1);
    }

    size_t pos = 0;
    while ((pos = path.find(separator)) != std::string_view::npos) {
        tokens.emplace_back(path.substr(0, pos));
        path.remove_prefix(pos + 1);
    }
    if (!path.empty()) {
        tokens.emplace_back(path);
    }

    // Process RFC escapes
    for (auto& token : tokens) {
        size_t r_pos = 0;
        while ((r_pos = token.find('~', r_pos)) != std::string::npos && r_pos + 1 < token.size()) {
            if (token[r_pos + 1] == '1') {
                token.replace(r_pos, 2, "/");
            } else if (token[r_pos + 1] == '0') {
                token.replace(r_pos, 2, "~");
            }
            r_pos++;
        }
    }
    return tokens;
}

// Free standing pointer finder matching your requested signature
template <typename value_type> //
Location<value_type> locate_impl(value_type& root, std::string_view path, char separator,
                                 bool create) {
    if (path.empty() || (path.size() == 1 && path[0] == separator)) {
        return Location<value_type>(&root);
    }

    std::vector<std::string> tokens = tokenize(path, separator);
    value_type* parent = nullptr;
    std::string key;
    size_t index = 0;
    value_type* current = &root;

    for (const auto& token : tokens) {
        parent = current;
        key = "";
        index = 0;
        if (current->is_object()) {
            auto& obj = const_cast<boost::json::object&>(current->as_object());
            auto it = obj.find(token);
            if (it == obj.end()) {
                if (!create) {
                    current = nullptr;
                } else {
                    it = obj.emplace(token, boost::json::object{}).first;
                    current = &it->value();
                }
            } else {
                current = &it->value();
            }
            key = token;
        } else if (current->is_array()) {
            auto& arr = current->as_array();
            if (token == "-")
                return Location<value_type>{};

            size_t target_index = 0;
            auto [ptr, ec] =
                std::from_chars(token.data(), token.data() + token.size(), target_index);

            if (ec != std::errc{} || ptr != token.data() + token.size())
                return Location<value_type>{};
            if (token.size() > 1 && token[0] == '0')
                return Location<value_type>{};
            if (target_index >= arr.size()) {
                if (create) {
                    boost::json::array& ja = const_cast<boost::json::array&>(arr);
                    for (size_t size = arr.size(); !(target_index < size); size++) {
                        ja.emplace_back(value_type{});
                    }
                } else {
                    return Location<value_type>{};
                }
            }
            current = &arr[target_index];
            index = target_index;
        } else if constexpr (!std::is_const_v<value_type>) {
            if (create && current->is_null()) {
                *current = boost::json::object{};
                key = token;
            } else {
                return Location<value_type>{};
            }
        } else {
            return Location<value_type>{};
        }
    }

    return Location<value_type>(parent, key, index, current);
}

Location<boost::json::value> locate(boost::json::value& root, std::string_view path, char separator,
                                    bool create) {
    return locate_impl<boost::json::value>(root, path, separator, create);
}

Location<const boost::json::value> locate_const(const boost::json::value& root,
                                                std::string_view path, char separator) {
    return locate_impl<const boost::json::value>(root, path, separator, false);
}

} // namespace bas::json