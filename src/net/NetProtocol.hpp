#ifndef NETPROTOCOL_H
#define NETPROTOCOL_H

#include "URL.hpp"

#include "../io/InputStream.hpp"
#include "../io/OutputStream.hpp"
#include "../io/Reader.hpp"
#include "../io/Writer.hpp"

#include <memory>

class NetProtocol {
public:
    virtual ~NetProtocol() = default;

    /** Protocol name: "http", "https", "ftp", "file", etc. */
    virtual std::string getProtocolName() const = 0;

    /** Open input stream for the given URL. Returns null if not supported or error. */
    virtual std::unique_ptr<InputStream> newInputStream(const URL& url) = 0;

    /** Open random input stream for the given URL. Returns null if not supported or error. Default: nullptr. */
    virtual std::unique_ptr<RandomInputStream> newRandomInputStream(const URL& url);

    /** Open output stream for the given URL. append: if false, truncate. Returns null if not supported or error. */
    virtual std::unique_ptr<OutputStream> newOutputStream(const URL& url, bool append = false) = 0;

    /** Open reader for the given URL. Returns null if not supported or error. */
    virtual std::unique_ptr<Reader> newReader(const URL& url, std::string_view charset = "UTF-8");

    /** Open random reader for the given URL. Returns null if not supported or error. */
    virtual std::unique_ptr<RandomReader> newRandomReader(const URL& url, std::string_view charset = "UTF-8");

    /** Open writer for the given URL. Returns null if not supported or error. */
    virtual std::unique_ptr<Writer> newWriter(const URL& url, std::string_view charset = "UTF-8");

};

#endif // NETPROTOCOL_H
