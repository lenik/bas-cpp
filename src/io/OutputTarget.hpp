#ifndef OUTPUTTARGET_H
#define OUTPUTTARGET_H

#include "OutputStream.hpp"
#include "WriteStream.hpp"

#include <memory>
#include <string>

class URI;
class URL;

class OutputTarget {
public:
    virtual ~OutputTarget() = default;

    /** Create a new byte output stream. \a append: if false, truncate/overwrite. Caller owns the returned stream. */
    virtual std::unique_ptr<OutputStream> newOutputStream(bool append = false) = 0;

    /** Create a new character writer with encoding. \a append: if false, truncate/overwrite. Caller owns the returned stream. */
    virtual std::unique_ptr<IWriteStream> newWriter(bool append = false) = 0;

    /** URI for this target (e.g. file:///path). */
    virtual URI getURI() const = 0;

    /** URL for this target. */
    virtual URL getURL() const = 0;

    /** Display name (e.g. file path). */
    virtual std::string getName() const = 0;

    /** Character set for encoding (e.g. "UTF-8"). */
    virtual std::string getCharset() const = 0;
};

#endif // OUTPUTTARGET_H
