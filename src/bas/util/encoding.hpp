#ifndef ENCODING_HPP
#define ENCODING_HPP

#include <sstream>
#include <string>

namespace encoding {

template <typename T> inline std::string to_hex(T value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

} // namespace encoding

#endif // ENCODING_HPP
