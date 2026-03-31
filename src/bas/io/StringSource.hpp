#ifndef STRINGSOURCE_H
#define STRINGSOURCE_H

#include "InputSource.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <string>

/** InputSource that reads from a string (bytes interpreted with charset). */
class StringSource : public EncodingInputSource {
public:
    explicit StringSource(const std::string& data,
                               std::string_view charset = "UTF-8");

    std::unique_ptr<Reader> newReader() override;
    std::unique_ptr<RandomReader> newRandomReader() override;

    std::string getCharset() const override { return m_charset; }
    void setCharset(const std::string& charset) { m_charset = charset; }

    std::string getName() const override { return m_name; }
    URI getURI() const override { return m_uri; }
    URL getURL() const override { return m_url; }

    void setName(const std::string& name) { m_name = name; }
    void setURI(const URI& uri) { m_uri = uri; }
    void setURL(const URL& url) { m_url = url; }

private:
    std::string m_data;
    std::string m_charset;

    // helper fields for convenience
    URI m_uri;
    URL m_url;
    std::string m_name;
};

#endif // STRINGSOURCE_H
