#include "urls.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace url {

std::string encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        // Keep alphanumeric and other allowed characters as-is
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase << '%' << std::setw(2)
                << static_cast<int>(static_cast<unsigned char>(c));
    }

    return escaped.str();
}

} // namespace url