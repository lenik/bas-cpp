#include "Uint8ArraySource.hpp"

#include "Uint8ArrayInputStream.hpp"

#include <algorithm>
#include <memory>

Uint8ArraySource::Uint8ArraySource(const std::vector<uint8_t>& array,
                                             size_t off, size_t len,
                                             std::string_view charset)
    : m_array(&array)
    , m_off(off)
    , m_len(len == static_cast<size_t>(-1) ? (array.size() - off) : len)
    , m_charset(charset.empty() ? "UTF-8" : charset)
{
    if (m_off + m_len > array.size())
        m_len = m_off < array.size() ? array.size() - m_off : 0;
}

std::unique_ptr<InputStream> Uint8ArraySource::newInputStream() {
    return newRandomInputStream();
}

std::unique_ptr<RandomInputStream> Uint8ArraySource::newRandomInputStream() {
    // Clamp offset so we never pass off > size to Uint8ArrayInputStream (undefined behavior).
    size_t off = std::min(m_off, m_array->size());
    return std::make_unique<Uint8ArrayInputStream>(*m_array, off, m_len);
}

URI Uint8ArraySource::getURI() const { return m_uri; }

URL Uint8ArraySource::getURL() const { return m_url; }

std::string Uint8ArraySource::getName() const { return m_name; }

std::string Uint8ArraySource::getCharset() const { return m_charset; }
