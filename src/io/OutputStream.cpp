#include "OutputStream.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

size_t OutputStream::write(const uint8_t* buf, size_t off, size_t len) {
    if (len == 0) return 0;
    if (!buf) throw std::invalid_argument("null buf");
    for (size_t i = 0; i < len; ++i) {
        int byt = static_cast<int>(buf[off + i]);
        if (!write(byt))
            return i;
    }
    return len;
}

size_t OutputStream::write(std::vector<uint8_t> data) {
    if (data.empty()) return 0;
    return write(data.data(), 0, data.size());
}
