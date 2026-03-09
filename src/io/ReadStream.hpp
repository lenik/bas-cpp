#ifndef READSTREAM_H
#define READSTREAM_H

#include "InputStream.hpp"
#include "Reader.hpp"

#include <cstdint>
#include <string>
#include <vector>

class IReadStream : public InputStream, public Reader {
public:
    virtual ~IReadStream() = default;

    enum class MalformAction {
        Skip,
        Replace,
        Error
    };

    virtual std::string getEncoding() const = 0;
    virtual MalformAction malformAction() const = 0;
    virtual void setMalformAction(MalformAction action) = 0;
    virtual int malformReplacement() const = 0;
    virtual void setMalformReplacement(int codePoint) = 0;

    // Bytes from the last malformed character sequence (if any).
    virtual std::vector<uint8_t> getLastMalformedBytes() const = 0;

    virtual void close() = 0;
};

#endif // READSTREAM_H

