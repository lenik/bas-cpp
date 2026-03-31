#ifndef INPUTSOURCE_H
#define INPUTSOURCE_H

#include "InputStream.hpp"
#include "Reader.hpp"

#include <memory>
#include <string>

class URI;
class URL;

class InputSource {
public:
    virtual ~InputSource() = default;

    /** Create a new byte input stream. Caller owns the returned stream. */
    virtual std::unique_ptr<InputStream> newInputStream() = 0;

    /** return null if not supported */
    virtual std::unique_ptr<RandomInputStream> newRandomInputStream() {
        return nullptr;
    }

    /** Create a new character reader with decoding. Caller owns the returned stream. */
    virtual std::unique_ptr<Reader> newReader() = 0;

    /** return null if not supported */
    virtual std::unique_ptr<RandomReader> newRandomReader() {
        return nullptr;
    }

    /** URI for this source (e.g. file:///path). */
    virtual URI getURI() const = 0;

    /** URL for this source. */
    virtual URL getURL() const = 0;

    /** Display name (e.g. file path). */
    virtual std::string getName() const = 0;

    /** Character set for decoding (e.g. "UTF-8"). */
    virtual std::string getCharset() const = 0;
};

class DecodingInputSource : public InputSource {
public:
    ~DecodingInputSource() override = default;

    explicit DecodingInputSource(std::string_view charset = "UTF-8");

    std::unique_ptr<Reader> newReader() override;
    std::unique_ptr<RandomReader> newRandomReader() override;

private:
    std::string m_charset;
};

class EncodingInputSource : public InputSource {
public:
    ~EncodingInputSource() override = default;

    explicit EncodingInputSource(std::string_view charset = "UTF-8");

    std::unique_ptr<InputStream> newInputStream() override;
    std::unique_ptr<RandomInputStream> newRandomInputStream() override;

private:
    std::string m_charset;
};

#endif // INPUTSOURCE_H
