#include "URLOutputTarget.hpp"

#include "ProtocolRegistry.hpp"

#include "../io/EncodingWriteStreamToOutputStream.hpp"

#include <memory>

URLOutputTarget::URLOutputTarget(const URL& url, std::string_view charset)
    : m_url(url)
    , m_charset(charset.empty() ? "UTF-8" : charset)
{
}

URLOutputTarget::URLOutputTarget(URL&& url, std::string_view charset)
    : m_url(std::move(url))
    , m_charset(charset.empty() ? "UTF-8" : charset)
{
}

std::unique_ptr<OutputStream> URLOutputTarget::newOutputStream(bool append) {
    NetProtocol* p = ProtocolRegistry::instance().getProtocolFor(m_url);
    if (!p) return nullptr;
    return p->newOutputStream(m_url, append);
}

std::unique_ptr<IWriteStream> URLOutputTarget::newWriter(bool append) {
    std::unique_ptr<OutputStream> out = newOutputStream(append);
    if (!out) return nullptr;
    return std::make_unique<EncodingWriteStreamToOutputStream>(std::move(out), m_charset);
}

URI URLOutputTarget::getURI() const {
    return m_url.toURI();
}

URL URLOutputTarget::getURL() const {
    return m_url;
}

std::string URLOutputTarget::getName() const {
    return m_url.spec();
}

std::string URLOutputTarget::getCharset() const {
    return m_charset;
}
