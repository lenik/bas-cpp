#include "boost_json.hpp"

#include <cstring>

namespace boost::json {

bool get_bool(const boost::json::object &json, const char *key,
              bool default_value = false);

int get_int(const boost::json::object &json, const char *key,
            int default_value = 0);

unsigned int get_uint(const boost::json::object &json, const char *key,
                      unsigned int default_value = 0);

int64_t get_int64(const boost::json::object &json, const char *key,
                  int64_t default_value = 0);

uint64_t get_uint64(const boost::json::object &json, const char *key,
                    uint64_t default_value = 0);

double get_double(const boost::json::object &json, const char *key,
                  double default_value = 0);

std::string get_string(const boost::json::object &json, const char *key,
                       const char *default_value = "");

} // namespace boost::json