#include "U32stringTarget.hpp"

#include "EncodingWriteStreamToOutputStream.hpp"
#include "OutputStream.hpp"

#include "../util/unicode.hpp"

#include <cstring>
#include <memory>
#include <vector>

namespace {

class U32TargetOutputStream : public OutputStream {
public:
    U32TargetOutputStream(U32stringTarget* target, bool append)
        : m_target(target)
        , m_closed(false)
    {
        if (!append && m_target)
            m_target->clear();
    }

    bool write(int byte) override {
        if (m_closed || !m_target) return false;
        m_buffer.push_back(static_cast<uint8_t>(byte & 0xFF));
        return true;
    }

    size_t write(const uint8_t* buf, size_t off, size_t len) override {
        if (m_closed || !m_target || !buf || len == 0) return 0;
        m_buffer.insert(m_buffer.end(), buf + off, buf + off + len);
        return len;
    }

    void flush() override {}

    void close() override {
        if (m_closed || !m_target) return;
        m_closed = true;
        if (m_buffer.empty()) return;
        std::string charset = m_target->getCharset();
        std::string bytesAsStr(m_buffer.begin(), m_buffer.end());
        std::string utf8;
        if (charset == "UTF-8" || charset.empty()) {
            utf8 = bytesAsStr;
        } else {
            auto unicode = unicode::fromEncoding(bytesAsStr, charset);
            utf8 = unicode::convertToUTF8(unicode);
        }

        const uint8_t* p = reinterpret_cast<const uint8_t*>(utf8.data());
        const uint8_t* end = p + utf8.size();
        while (p < end) {
            auto [cp, n] = unicode::utf8DecodeCodePoint(p, end);
            if (n <= 0 || cp < 0) break;
            m_target->str().push_back(static_cast<char32_t>(cp));
            p += n;
        }
    }

private:
    U32stringTarget* m_target;
    std::vector<uint8_t> m_buffer;
    bool m_closed;
};

} // namespace

U32stringTarget::U32stringTarget(std::string_view charset)
    : m_charset(charset.empty() ? "UTF-8" : charset)
{
}

std::unique_ptr<OutputStream> U32stringTarget::newOutputStream(bool append) {
    return std::make_unique<U32TargetOutputStream>(this, append);
}

std::unique_ptr<IWriteStream> U32stringTarget::newWriter(bool append) {
    if (!append) clear();
    return std::make_unique<EncodingWriteStreamToOutputStream>(
        std::make_unique<U32TargetOutputStream>(this, append), m_charset);
}
