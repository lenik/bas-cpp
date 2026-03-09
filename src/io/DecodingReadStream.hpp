#ifndef DECODINGREADSTREAM_H
#define DECODINGREADSTREAM_H

#include "InputStream.hpp"
#include "LAInputStream.hpp"
#include "ReadStream.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class LAInputStreamFromStream;

class DecodingReadStream : public IReadStream, virtual public ISeekable {
public:
    explicit DecodingReadStream(LAInputStream* in, std::string_view encoding = "UTF-8");
    /** Takes ownership of \a in and builds an internal LAInputStream. */
    explicit DecodingReadStream(std::unique_ptr<InputStream> in, std::string_view encoding = "UTF-8");
    ~DecodingReadStream() override;

    // Read single byte. Returns [0-255] or -1 for EOF.
    int read() override;

    // Read up to len bytes into buf+off. Returns bytes read or 0 on EOF.
    size_t read(uint8_t* buf, size_t off, size_t len) override;

    std::vector<uint8_t> readBytes(size_t len) override;
    std::vector<uint8_t> readBytesUntilEOF() override;
    std::vector<uint8_t> readRawLine() override;
    std::vector<uint8_t> readRawLineChopped() override;

    int64_t position() const override;
    bool canSeek(std::ios::seekdir dir = std::ios::beg) const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;
    int64_t skip(int64_t len) override;

    std::string getEncoding() const override { return m_encoding; }
    MalformAction malformAction() const override { return m_malformAction; }
    void setMalformAction(MalformAction action) override { m_malformAction = action; }
    int malformReplacement() const override { return m_malformReplacement; }
    void setMalformReplacement(int codePoint) override { m_malformReplacement = codePoint; }

    int readChar() override;
    std::vector<uint8_t> getLastMalformedBytes() const override { return m_lastMalformed; }

    // faster alternatives
    std::string readCharsUntilEOF() override;
    std::string readLine() override;
    
    void close() override;

private:
    
    // Discard decoded buffer and seek back to where decoding started
    void discardDecodedBuffer();
    
    // Decode a block of bytes into characters
    void decodeBlock(const std::vector<uint8_t>& bytes);

    // Decode a single UTF-8 code point from internal decoded buffer; 
    // returns -1 on EOF, -2 on malformed
    int decodeFromBuffer();

    LAInputStream* in;
    std::unique_ptr<LAInputStreamFromStream> m_ownedLA;  // when set, 'in' == m_ownedLA.get()
    std::string m_encoding;
    
    // Decoded character buffer
    std::vector<uint8_t> m_decodedBuffer;  // Decoded UTF-8 bytes
    int m_decodedBufferPos;      // Current position in decoded buffer
    
    // Position tracking for seek-back
    int64_t m_decodedStartPos;     // Stream position where decoding started
    int m_decodedBytesConsumed;   // Bytes consumed from stream for decoding
    
    std::vector<uint8_t> m_pendingMalformed;
    std::vector<uint8_t> m_lastMalformed;
    MalformAction m_malformAction;
    int m_malformReplacement;
    
    static const int BUFFER_SIZE = 4096;
};

#endif // DECODINGREADSTREAM_H