#include "LocalOutputTarget.hpp"

#include "EncodingWriteStreamToOutputStream.hpp"
#include "LocalOutputStream.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <memory>

LocalOutputTarget::LocalOutputTarget(const std::string& file, std::string_view charset)
    : m_file(file)
    , m_charset(charset.empty() ? "UTF-8" : charset)
{
}

std::unique_ptr<OutputStream> LocalOutputTarget::newOutputStream(bool append) {
    return std::make_unique<LocalOutputStream>(m_file, append);
}

std::unique_ptr<IWriteStream> LocalOutputTarget::newWriter(bool append) {
    return std::make_unique<EncodingWriteStreamToOutputStream>(
        std::make_unique<LocalOutputStream>(m_file, append), m_charset);
}

URI LocalOutputTarget::getURI() const {
    return URI::file(m_file);
}

URL LocalOutputTarget::getURL() const {
    return URL::local(m_file);
}

std::string LocalOutputTarget::getName() const {
    return m_file;
}

std::string LocalOutputTarget::getCharset() const {
    return m_charset;
}
