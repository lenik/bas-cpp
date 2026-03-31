#include "DataUriInputStream.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ios>
#include <sstream>

namespace {

static const char base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool base64Decode(const std::string& in, std::vector<uint8_t>& out) {
    out.clear();
    std::string s = in;
    s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)); }), s.end());
    int val = 0, valb = -8;
    for (unsigned char c : s) {
        if (c == '=') break;
        const char* p = std::strchr(base64Table, c);
        if (!p) return false;
        val = (val << 6) + (p - base64Table);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return true;
}

std::string percentDecode(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int a = 0, b = 0;
            if (s[i+1] >= '0' && s[i+1] <= '9') a = s[i+1] - '0';
            else if (s[i+1] >= 'A' && s[i+1] <= 'F') a = s[i+1] - 'A' + 10;
            else if (s[i+1] >= 'a' && s[i+1] <= 'f') a = s[i+1] - 'a' + 10;
            else { out += s[i]; continue; }
            if (s[i+2] >= '0' && s[i+2] <= '9') b = s[i+2] - '0';
            else if (s[i+2] >= 'A' && s[i+2] <= 'F') b = s[i+2] - 'A' + 10;
            else if (s[i+2] >= 'a' && s[i+2] <= 'f') b = s[i+2] - 'a' + 10;
            else { out += s[i]; continue; }
            out += static_cast<char>((a << 4) + b);
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

} // namespace

DataUriInputStream::DataUriInputStream(const std::string& spec, bool isDataUri)
    : m_position(0)
    , m_closed(false)
    , m_ok(false)
{
    if (isDataUri) parse(spec);
    else {
        m_data.assign(spec.begin(), spec.end());
        m_mimeType = "text/plain";
        m_ok = true;
    }
}

DataUriInputStream::DataUriInputStream(std::vector<uint8_t> payload)
    : m_data(std::move(payload))
    , m_mimeType("application/octet-stream")
    , m_position(0)
    , m_closed(false)
    , m_ok(true)
{
}

void DataUriInputStream::parse(const std::string& spec) {
    if (spec.size() < 5 || spec.substr(0, 5) != "data:") {
        m_ok = false;
        return;
    }
    size_t comma = spec.find(',', 5);
    if (comma == std::string::npos) {
        m_ok = false;
        return;
    }
    std::string_view params(spec.data() + 5, comma - 5);
    std::string_view payload = std::string_view(spec).substr(comma + 1);
    bool base64 = false;
    size_t semicolon = 0;
    if ((semicolon = params.find(';')) != std::string_view::npos) {
        m_mimeType = std::string(params.substr(0, semicolon));
        if (params.size() > semicolon + 7 && params.substr(semicolon + 1, 7) == "base64") base64 = true;
    } else {
        m_mimeType = params.empty() ? "text/plain" : std::string(params);
    }
    std::string decoded = percentDecode(payload);
    if (base64) {
        m_ok = base64Decode(decoded, m_data);
    } else {
        m_data.assign(decoded.begin(), decoded.end());
        m_ok = true;
    }
}

int DataUriInputStream::read() {
    if (m_closed || !m_ok || m_position >= m_data.size()) return -1;
    return static_cast<int>(m_data[m_position++]);
}

size_t DataUriInputStream::read(uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !m_ok || !buf) return 0;
    size_t remaining = m_data.size() - m_position;
    if (remaining == 0) return 0;
    size_t n = (len < remaining) ? len : remaining;
    std::memcpy(buf + off, m_data.data() + m_position, n);
    m_position += n;
    return n;
}

void DataUriInputStream::close() {
    m_closed = true;
}

int64_t DataUriInputStream::position() const {
    return m_position;
}

bool DataUriInputStream::seek(int64_t offset, std::ios::seekdir dir) {
    if (m_closed) return false;
    int64_t pos = 0;
    switch (dir) {
    case std::ios::beg:
        pos = offset;
        break;
    case std::ios::cur:
        pos = static_cast<int64_t>(m_position) + offset;
        break;
    case std::ios::end:
        pos = static_cast<int64_t>(m_data.size()) + offset;
        break;
    default:
        return false;
    }
    if (pos < 0 || static_cast<size_t>(pos) > m_data.size()) return false;
    m_position = static_cast<size_t>(pos);
    return true;
}
