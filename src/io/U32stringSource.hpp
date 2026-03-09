#ifndef U32STRINGSOURCE_H
#define U32STRINGSOURCE_H

#include "InputSource.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <string>

/** InputSource that reads from a UTF-32 string (code points). */
class U32stringSource : public EncodingInputSource {
public:
    explicit U32stringSource(const std::u32string& data,
                            std::string_view charset = "UTF-8");
    /** Decode UTF-8 \a data to UTF-32 and use as source. */
    explicit U32stringSource(const std::string& data,
                            std::string_view charset = "UTF-8");

    ~U32stringSource() override = default;
    
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

    /** The underlying UTF-32 data. */
    const std::u32string& data() const { return m_data; }

private:
    std::u32string m_data;
    std::string m_charset;

    URI m_uri;
    URL m_url;
    std::string m_name;
};

#endif // U32STRINGSOURCE_H
