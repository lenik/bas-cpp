#ifndef BAS_REG_STRINGS_HPP
#define BAS_REG_STRINGS_HPP

#include <string>
#include <string_view>
#include <vector>

namespace bas::util {

std::string trim(std::string s);
/** Remove trailing '/' and '\\'. */
std::string trimTrailingSlash(std::string_view path);
bool iequals(const std::string& a, const std::string& b);

std::vector<std::string> split(const std::string& s, char delimiter);
std::vector<std::string_view> split(std::string_view str, char delimiter);

std::string join(const std::vector<std::string>& strings, std::string_view delimiter);
std::string join(const std::vector<std::string>& strings, char delimiter);

std::string& replace(std::string& s, char old_char, char new_char);
inline std::string replace(std::string_view s, char old_char, char new_char) {
    std::string out(s);
    replace(out, old_char, new_char);
    return out;
}

} // namespace bas::util

#endif // BAS_REG_STRINGS_HPP
