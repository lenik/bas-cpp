#ifndef U32STRINGTARGET_H
#define U32STRINGTARGET_H

#include "OutputTarget.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <string>

/** OutputTarget that accumulates output into a UTF-32 string. */
class U32stringTarget : public OutputTarget {
public:
    explicit U32stringTarget(std::string_view charset = "UTF-8");

    ~U32stringTarget() override = default;

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

    /** Clear the buffer (e.g. before newOutputStream(false) or newWriter(false)). */
    void clear() { m_data.clear(); }

    /** The accumulated UTF-32 string. */
    const std::u32string& str() const { return m_data; }
    std::u32string& str() { return m_data; }

private:
    std::u32string m_data;
    std::string m_charset;

    URI m_uri;
    URL m_url;
    std::string m_name;
};

#endif // U32STRINGTARGET_H
