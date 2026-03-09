#include "URLInputSource.hpp"

#include "ProtocolRegistry.hpp"

#include <memory>

URLInputSource::URLInputSource(const URL& url, std::string_view charset)
    : m_url(url)
    , m_charset(charset.empty() ? "UTF-8" : charset)
{
}

URLInputSource::URLInputSource(URL&& url, std::string_view charset)
    : m_url(std::move(url))
    , m_charset(charset.empty() ? "UTF-8" : charset)
{
}

URI URLInputSource::getURI() const {
    return m_url.toURI();
}

URL URLInputSource::getURL() const {
    return m_url;
}

std::string URLInputSource::getName() const {
    return m_url.spec();
}

std::string URLInputSource::getCharset() const {
    return m_charset;
}

std::unique_ptr<InputStream> URLInputSource::newInputStream() {
    NetProtocol* p = ProtocolRegistry::instance().getProtocolFor(m_url);
    if (!p) return nullptr;
    return p->newInputStream(m_url);
}

std::unique_ptr<RandomInputStream> URLInputSource::newRandomInputStream() {
    NetProtocol* p = ProtocolRegistry::instance().getProtocolFor(m_url);
    if (!p) return nullptr;
    return p->newRandomInputStream(m_url);
}
