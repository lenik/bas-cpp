#include "json.hpp"

#include <cassert>

namespace boost::json {

bool get_bool(const boost::json::object &json, const char *key,
              bool default_value) {
  if (!key) throw std::invalid_argument("null key");
  if (json.if_contains(key))
    return json.at(key).as_bool();
  else
    return default_value;
}

int get_int(const boost::json::object &json, const char *key,
            int default_value) {
  if (!key) throw std::invalid_argument("null key");
  if (json.if_contains(key)) {
    int64_t val = json.at(key).as_int64();
    return val;
  } else
    return default_value;
}

unsigned int get_uint(const boost::json::object &json, const char *key,
                      unsigned int default_value) {
  if (!key) throw std::invalid_argument("null key");
  if (json.if_contains(key)) {
    uint64_t val = json.at(key).as_uint64();
    return val;
  } else
    return default_value;
}

int64_t get_int64(const boost::json::object &json, const char *key,
                  int64_t default_value) {
  if (!key) throw std::invalid_argument("null key");
  if (json.if_contains(key))
    return json.at(key).as_int64();
  else
    return default_value;
}

uint64_t get_uint64(const boost::json::object &json, const char *key,
                    uint64_t default_value) {
  if (!key) throw std::invalid_argument("null key");
  if (json.if_contains(key))
    return json.at(key).as_uint64();
  else
    return default_value;
}

double get_double(const boost::json::object &json, const char *key,
                  double default_value) {
  if (!key) throw std::invalid_argument("null key");
  if (json.if_contains(key))
    return json.at(key).as_double();
  else
    return default_value;
}

std::string get_string(const boost::json::object &json, const char *key,
                       const char *default_value) {
  if (!key) throw std::invalid_argument("null key");
  if (!default_value) throw std::invalid_argument("null default_value");
  if (json.if_contains(key)) {
    const boost::json::string &js = json.at(key).as_string();
    return std::string(js.begin(), js.end());
  } else {
    return default_value ? default_value : "";
  }
}

} // namespace boost::json