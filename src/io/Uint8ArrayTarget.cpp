#include "Uint8ArrayTarget.hpp"

#include "EncodingWriteStreamToOutputStream.hpp"
#include "OutputStream.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <algorithm>
#include <cstring>
#include <memory>

namespace {

class Uint8ArrayOutputStream : public OutputStream {
public:
    explicit Uint8ArrayOutputStream(Uint8ArrayTarget* target)
        : m_target(target)
        , m_closed(false)
    {}
    bool write(int byte) override {
        if (m_closed || !m_target) return false;
        uint8_t b = static_cast<uint8_t>(byte & 0xFF);
        m_target->append(&b, 1);
        return true;
    }
    size_t write(const uint8_t* buf, size_t off, size_t len) override {
        if (m_closed || !m_target || !buf || len == 0) return 0;
        m_target->append(buf + off, len);
        return len;
    }
    void flush() override {}
    void close() override { m_closed = true; }
private:
    Uint8ArrayTarget* m_target;
    bool m_closed;
};

} // namespace

Uint8ArrayTarget::Uint8ArrayTarget(size_t capacity, std::string_view charset)
    : m_charset(charset.empty() ? "UTF-8" : charset)
{
    if (capacity > 0)
        m_data.reserve(capacity);
}

void Uint8ArrayTarget::ensureCapacity(size_t n) {
    if (m_data.capacity() < n)
        m_data.reserve(std::max(n, m_data.capacity() * 2));
}

void Uint8ArrayTarget::append(const uint8_t* buf, size_t len) {
    if (!buf || len == 0) return;
    ensureCapacity(m_data.size() + len);
    m_data.insert(m_data.end(), buf, buf + len);
}

std::unique_ptr<OutputStream> Uint8ArrayTarget::newOutputStream(bool append) {
    if (!append) clear();
    return std::make_unique<Uint8ArrayOutputStream>(this);
}

std::unique_ptr<IWriteStream> Uint8ArrayTarget::newWriter(bool append) {
    if (!append) clear();
    return std::make_unique<EncodingWriteStreamToOutputStream>(
        std::make_unique<Uint8ArrayOutputStream>(this), m_charset);
}

URI Uint8ArrayTarget::getURI() const { return URI(); }
URL Uint8ArrayTarget::getURL() const { return URL(); }
std::string Uint8ArrayTarget::getName() const { return "memory"; }
std::string Uint8ArrayTarget::getCharset() const { return m_charset; }
