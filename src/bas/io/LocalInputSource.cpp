#include "LocalInputSource.hpp"

#include "LocalInputStream.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <memory>

#include <unicode/unistr.h>

LocalInputSource::LocalInputSource(const std::string& file, std::string_view charset)
    : DecodingInputSource(charset.empty() ? "UTF-8" : charset)
    , m_file(file)
    , m_charset(charset)
{
}

std::unique_ptr<InputStream> LocalInputSource::newInputStream() {
    return std::make_unique<LocalInputStream>(m_file);
}

std::unique_ptr<RandomInputStream> LocalInputSource::newRandomInputStream() {
    return std::make_unique<LocalInputStream>(m_file);
}

URI LocalInputSource::getURI() const {
    return URI::file(m_file);
}

URL LocalInputSource::getURL() const {
    return URL::local(m_file);
}

std::string LocalInputSource::getName() const {
    return m_file;
}

std::string LocalInputSource::getCharset() const {
    return m_charset;
}
