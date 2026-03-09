#ifndef UINT8ARRAYSOURCE_H
#define UINT8ARRAYSOURCE_H

#include "InputSource.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <cstddef>
#include <string>
#include <vector>

/** InputSource that reads from a slice of a byte array. Does not own the array. */
class Uint8ArraySource : public DecodingInputSource {
public:
    /** \a len defaults to array.size() - off. */
    Uint8ArraySource(const std::vector<uint8_t>& array,
                          size_t off = 0,
                          size_t len = static_cast<size_t>(-1),
                          std::string_view charset = "UTF-8");

    ~Uint8ArraySource() override {}

    std::unique_ptr<InputStream> newInputStream() override;
    std::unique_ptr<RandomInputStream> newRandomInputStream() override;

    std::string getCharset() const override;
    void setCharset(const std::string& charset) { m_charset = charset; }

    URI getURI() const override;
    URL getURL() const override;
    std::string getName() const override;

    void setName(const std::string& name) { m_name = name; }
    void setURI(const URI& uri) { m_uri = uri; }
    void setURL(const URL& url) { m_url = url; }

private:
    const std::vector<uint8_t>* m_array;
    size_t m_off;
    size_t m_len;
    std::string m_charset;

    // helper fields for convenience
    URI m_uri;
    URL m_url;
    std::string m_name;
};

#endif // UINT8ARRAYSOURCE_H
