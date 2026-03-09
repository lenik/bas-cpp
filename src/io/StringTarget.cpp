#include "StringTarget.hpp"

#include "EncodingWriteStreamToOutputStream.hpp"
#include "OutputStream.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <cstring>
#include <memory>

namespace {

class StringTargetOutputStream : public OutputStream {
public:
    explicit StringTargetOutputStream(StringTarget* target)
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
    StringTarget* m_target;
    bool m_closed;
};

} // namespace

StringTarget::StringTarget(std::string_view charset)
    : m_charset(charset.empty() ? "UTF-8" : charset)
{
}

void StringTarget::append(const uint8_t* buf, size_t len) {
    if (!buf || len == 0) return;
    m_buffer.insert(m_buffer.end(), buf, buf + len);
}

std::string StringTarget::str() const {
    return std::string(reinterpret_cast<const char*>(m_buffer.data()), m_buffer.size());
}

std::unique_ptr<OutputStream> StringTarget::newOutputStream(bool append) {
    if (!append) clear();
    return std::make_unique<StringTargetOutputStream>(this);
}

std::unique_ptr<IWriteStream> StringTarget::newWriter(bool append) {
    if (!append) clear();
    return std::make_unique<EncodingWriteStreamToOutputStream>(
        std::make_unique<StringTargetOutputStream>(this), m_charset);
}
