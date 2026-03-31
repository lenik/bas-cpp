#ifndef STRINGTARGET_H
#define STRINGTARGET_H

#include "OutputTarget.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <string>
#include <vector>

/** OutputTarget that accumulates output into a string. */
class StringTarget : public OutputTarget {
public:
    explicit StringTarget(std::string_view charset = "UTF-8");

    std::unique_ptr<OutputStream> newOutputStream(bool append = false) override;
    std::unique_ptr<IWriteStream> newWriter(bool append = false) override;

    std::string getCharset() const override { return m_charset; }
    void setCharset(const std::string& charset) { m_charset = charset; }

    URI getURI() const override { return m_uri; }
    URL getURL() const override { return m_url; }
    std::string getName() const override { return m_name; }
    
    void setURI(const URI& uri) { m_uri = uri; }
    void setURL(const URL& url) { m_url = url; }
    void setName(const std::string& name) { m_name = name; }

    /** Clear the buffer (e.g. before newOutputStream(false)). */
    void clear() { m_buffer.clear(); }

    /** Get the accumulated string (UTF-8 bytes interpreted as string). */
    std::string str() const;

    /** Append raw bytes (used by the stream returned from newOutputStream). */
    void append(const uint8_t* buf, size_t len);

private:
    std::string m_charset;
    std::vector<uint8_t> m_buffer;

    // helper fields for convenience
    URI m_uri;
    URL m_url;
    std::string m_name;
};

#endif // STRINGTARGET_H
