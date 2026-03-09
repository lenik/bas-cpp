#include "DecodingReadStream.hpp"

#include "LAInputStreamFromStream.hpp"

#include "../util/unicode.hpp"

#include <cstdint>
#include <utility>

DecodingReadStream::DecodingReadStream(LAInputStream* in, std::string_view encoding)
    : in(in)
    , m_ownedLA(nullptr)
    , m_encoding(encoding.empty() ? "UTF-8" : encoding)
    , m_decodedBufferPos(0)
    , m_decodedStartPos(-1)
    , m_decodedBytesConsumed(0)
    , m_malformAction(MalformAction::Replace)
    , m_malformReplacement('?')
{
}

DecodingReadStream::DecodingReadStream(std::unique_ptr<InputStream> in, std::string_view encoding)
    : in(nullptr)
    , m_ownedLA(std::make_unique<LAInputStreamFromStream>(std::move(in)))
    , m_encoding(encoding.empty() ? "UTF-8" : encoding)
    , m_decodedBufferPos(0)
    , m_decodedStartPos(-1)
    , m_decodedBytesConsumed(0)
    , m_malformAction(MalformAction::Replace)
    , m_malformReplacement('?')
{
    this->in = m_ownedLA.get();
}

DecodingReadStream::~DecodingReadStream() = default;

int DecodingReadStream::read() {
    // If we were decoding, discard decoded buffer and seek back
    if (m_decodedStartPos >= 0) {
        discardDecodedBuffer();
    }
    return in->read();
}

size_t DecodingReadStream::read(uint8_t* buf, size_t off, size_t len) {
    if (m_decodedStartPos >= 0) {
        discardDecodedBuffer();
    }
    return in->read(buf, off, len);
}

std::vector<uint8_t> DecodingReadStream::readRawLine() {
    if (m_decodedStartPos >= 0) {
        discardDecodedBuffer();
    }
    return in->readRawLine();
}

std::vector<uint8_t> DecodingReadStream::readRawLineChopped() {
    if (m_decodedStartPos >= 0) {
        discardDecodedBuffer();
    }
    return in->readRawLineChopped();
}

std::string DecodingReadStream::readCharsUntilEOF() {
    std::string out;
    while (true) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
    }
    return out;
}

void DecodingReadStream::discardDecodedBuffer() {
    if (m_decodedStartPos >= 0 && m_decodedBytesConsumed > 0) {
        // Seek back to where decoding started
        int64_t targetPos = m_decodedStartPos;
        if (in->seek(targetPos)) {
            // Reset buffer state
            in->discardPrefetch();
            m_decodedBuffer.clear();
            m_decodedBufferPos = 0;
            m_decodedStartPos = -1;
            m_decodedBytesConsumed = 0;
        }
    }
}

void DecodingReadStream::decodeBlock(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return;
    // Currently assume UTF-8; if different encoding is needed, extend here.
    m_decodedBuffer.insert(m_decodedBuffer.end(), bytes.begin(), bytes.end());
}

int DecodingReadStream::readChar() {
    int cp = decodeFromBuffer();
    if (cp >= 0 || cp == -2)
        return cp;
    // Buffer empty (cp == -1): try to read more from stream before returning EOF
    // Need to decode more. Start decoding from current position
    if (m_decodedStartPos < 0) {
        m_decodedStartPos = in->position();
        m_decodedBytesConsumed = 0;
    }
    
    // Read a block of bytes for decoding
    std::vector<uint8_t> block;
    const int DECODE_BLOCK_SIZE = 512;  // Decode in smaller blocks for better character boundary handling
    
    while (static_cast<int>(block.size()) < DECODE_BLOCK_SIZE) {
        int byte = in->read();
        if (byte == -1) {
            if (block.empty()) {
                return -1;  // EOF
            }
            break;  // EOF but we have some data
        }
        block.push_back(static_cast<uint8_t>(byte));
        m_decodedBytesConsumed++;
    }
    
    if (block.empty()) {
        return -1;  // EOF
    }
    
    // Decode the block
    decodeBlock(block);
    
    // Return first decoded character
    if (int cp = decodeFromBuffer(); cp >= -1 || cp == -2) {
        return cp;
    }
    
    return -1;
}

std::vector<uint8_t> DecodingReadStream::readBytes(size_t len) {
    if (len <= 0) return {};
    
    // If we were decoding, discard decoded buffer and seek back
    if (m_decodedStartPos >= 0) {
        discardDecodedBuffer();
    }
    return in->readBytes(len);
}

std::vector<uint8_t> DecodingReadStream::readBytesUntilEOF() {
    // If we were decoding, discard decoded buffer and seek back
    if (m_decodedStartPos >= 0) {
        discardDecodedBuffer();
    }
    return in->readBytesUntilEOF();
}

std::string DecodingReadStream::readLine() {
    std::string out;
    while (true) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
        if (cp == '\n') break;
    }
    return out;
}

int64_t DecodingReadStream::position() const {
    if (m_decodedStartPos >= 0) {
        // We're in decoding mode, position is where decoding started
        return m_decodedStartPos;
    }
    // Position is stream position minus bytes remaining in buffer
    return in->position();
}

bool DecodingReadStream::canSeek(std::ios::seekdir dir) const {
    return in ? in->canSeek(dir) : false;
}

bool DecodingReadStream::seek(int64_t offset, std::ios::seekdir dir) {
    if (m_decodedStartPos >= 0) {
        discardDecodedBuffer();
    }
    return in->seek(offset, dir);
}

int64_t DecodingReadStream::skip(int64_t len) {
    if (len <= 0) return 0;
    // If we're in decoding mode, discard decoded buffer first
    if (m_decodedStartPos >= 0) {
        discardDecodedBuffer();
    }
    return in->skip(len);
}

void DecodingReadStream::close() {
    in->close();
}

int DecodingReadStream::decodeFromBuffer() {
    if (m_decodedBufferPos >= static_cast<int>(m_decodedBuffer.size())) {
        return -1;
    }
    auto* start = m_decodedBuffer.data() + m_decodedBufferPos;
    auto* end = m_decodedBuffer.data() + m_decodedBuffer.size();
    auto [cp, consumed] = unicode::utf8DecodeCodePoint(start, end);
    if (consumed <= 0) {
        return -1;
    }
    m_decodedBufferPos += consumed;
    if (cp == -2) {
        m_lastMalformed.assign(start, start + consumed);
        switch (m_malformAction) {
        case MalformAction::Skip:
            return decodeFromBuffer(); // try next
        case MalformAction::Replace: {
            std::vector<uint8_t> rep;
            unicode::utf8EncodeCodePoint(m_malformReplacement, rep);
            // replace current malformed with replacement
            if (!rep.empty()) {
                // move buffer pos back and insert replacement as decoded buffer
                m_decodedBufferPos -= consumed;
                m_decodedBuffer.erase(m_decodedBuffer.begin() + m_decodedBufferPos,
                                      m_decodedBuffer.begin() + m_decodedBufferPos + consumed);
                m_decodedBuffer.insert(m_decodedBuffer.begin() + m_decodedBufferPos, rep.begin(), rep.end());
                // decode replacement
                return decodeFromBuffer();
            }
            return m_malformReplacement;
        }
        case MalformAction::Error:
        default:
            return -2;
        }
    }
    return cp;
}
