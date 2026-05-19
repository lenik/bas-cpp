#ifndef BAS_SCRIPT_JSON_HPP
#define BAS_SCRIPT_JSON_HPP

#include "boost_json.hpp"

#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace boost::json {

bool get_bool(const boost::json::object& json, const char* key, bool default_value = false);

int get_int(const boost::json::object& json, const char* key, int default_value = 0);

unsigned int get_uint(const boost::json::object& json, const char* key,
                      unsigned int default_value = 0);

int64_t get_int64(const boost::json::object& json, const char* key, int64_t default_value = 0);

uint64_t get_uint64(const boost::json::object& json, const char* key, uint64_t default_value = 0);

double get_double(const boost::json::object& json, const char* key, double default_value = 0);

std::string get_string(const boost::json::object& json, const char* key,
                       const char* default_value = "");

} // namespace boost::json

namespace bas::json {

std::string json_escape(const std::string& in);
std::string json_unescape(std::string_view s);

std::vector<std::string> tokenize(std::string_view path, char separator = '/');

template <typename value_type> //
struct Location {
    value_type* parent;
    std::string key;
    size_t index{0};
    value_type* node;

    Location(value_type* parent, std::string key, size_t index, value_type* node)
        : parent(parent), key(key), index(index), node(node) {}
    Location(value_type* parent, std::string key, value_type* node)
        : parent(parent), key(key), node(node) {}
    Location(value_type* parent, size_t index, value_type* node)
        : parent(parent), index(index), node(node) {}
    Location(value_type* node) : parent(nullptr), node(node) {}
    Location() : parent(nullptr), node(nullptr) {}

    bool is_object_field() const { return parent && parent->is_object(); }
    bool is_array_item() const { return parent && parent->is_array(); }
    bool is_root() const { return parent == nullptr; }
    bool is_null() const { return node == nullptr || node->is_null(); }

    std::optional<value_type> set(value_type& value) {
        if (is_object_field()) {
            auto& obj = parent->as_object(); // Capturing as a reference is mandatory!

            auto it = obj.find(key);
            if (it != obj.end()) {
                // Key exists: swap data to efficiently extract 'old' in O(1) time
                value_type old = std::move(it->value());
                it->value() = std::move(value);
                return old;
            } else {
                // Key doesn't exist: insert the new value, return empty/null json
                obj.emplace(key, std::move(value));
                return std::nullopt;
            }
        }

        if (is_array_item()) {
            auto& arr = parent->as_array(); // Capturing as a reference is mandatory!
            if (index >= arr.size()) {
                throw std::out_of_range("Array index out of bounds");
            }

            // Steal the old value and overwrite it in place using move semantics
            value_type old = std::move(arr[index]);
            arr[index] = std::move(value);
            return old;
        }

        if (is_root()) {
            if (!node) {
                throw std::runtime_error("Root node pointer is null");
            }
            value_type old = std::move(*node);
            *node = std::move(value);
            return old;
        }

        throw std::runtime_error("Invalid location state");
    }

    std::optional<value_type> erase() {
        if constexpr (std::is_const_v<value_type>) {
            return std::nullopt;
        } else {
            if (is_object_field()) {
                auto& obj = parent->as_object();
                auto it = obj.find(key);
                if (it == obj.end()) {
                    return std::nullopt; // Key wasn't actually there
                }

                // Move the value out before erasing to avoid deep copying
                value_type old = std::move(it->value());
                obj.erase(it);
                return old;
            }

            if (is_array_item()) {
                auto& arr = parent->as_array();
                if (index >= arr.size()) {
                    return std::nullopt; // Out of bounds safety check
                }

                // Move the value out of the array slot directly
                value_type old = std::move(arr[index]);
                arr.erase(arr.begin() + index);
                return old;
            }

            if (is_root()) {
                if (!node)
                    return std::nullopt;

                // Correctly steal the data from the root node
                value_type old = std::move(*node);

                // Reset the root node to a safe empty state (null JSON value)
                *node = nullptr;
                return old;
            }
        }
        return std::nullopt;
    }

}; // struct Location

Location<boost::json::value> locate(boost::json::value& root, std::string_view path,
                                    char separator = '/', bool create = false);

Location<const boost::json::value> locate_const(const boost::json::value& root,
                                                std::string_view path, char separator = '/');

} // namespace bas::json

#endif