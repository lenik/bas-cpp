#include "encoding.hpp"

#include <sstream>

namespace encoding {

template <typename T> std::string to_hex(T value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

} // namespace encoding
