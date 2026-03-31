#ifndef UINT8ARRAYTARGET_H
#define UINT8ARRAYTARGET_H

#include "OutputTarget.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <cstddef>
#include <string>
#include <vector>

/** OutputTarget that writes into a growable byte array. */
class Uint8ArrayTarget : public OutputTarget {
public:
    explicit Uint8ArrayTarget(size_t capacity = 0,
                                    std::string_view charset = "UTF-8");

    ~Uint8ArrayTarget() override = default;
    
    std::unique_ptr<OutputStream> newOutputStream(bool append = false) override;
    std::unique_ptr<IWriteStream> newWriter(bool append = false) override;

    std::string getCharset() const override;
    void setCharset(const std::string& charset) { m_charset = charset; }

    URI getURI() const override;
    URL getURL() const override;
    std::string getName() const override;

    void setName(const std::string& name) { m_name = name; }
    void setURI(const URI& uri) { m_uri = uri; }
    void setURL(const URL& url) { m_url = url; }

    /** Clear the buffer (e.g. before newOutputStream(false)). */
    void clear() { m_data.clear(); }

    /** The accumulated byte array. */
    const std::vector<uint8_t>& array() const { return m_data; }
    std::vector<uint8_t>& array() { return m_data; }

    /** Ensure buffer can hold at least \a n bytes (grows capacity as needed). */
    void ensureCapacity(size_t n);

    /** Append raw bytes (used by the stream returned from newOutputStream). */
    void append(const uint8_t* buf, size_t len);

private:
    std::vector<uint8_t> m_data;
    std::string m_charset;

    // helper fields for convenience
    URI m_uri;
    URL m_url;
    std::string m_name;
};

#endif // UINT8ARRAYTARGET_H
