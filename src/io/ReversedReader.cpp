#include "ReversedReader.hpp"

#include "../util/unicode.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

ReversedReader::ReversedReader(read_backward_func_t read_backward_func, size_t block_size) 
    : m_read_backward_func(read_backward_func)
    , m_block_size(block_size)
{
    m_buf = next(nullptr);
    m_bufptr = m_buf ? m_buf->size : 0;
    if (!m_optim)
        m_head = m_buf;
}

ReversedReader::~ReversedReader() {
    block_t *node = m_optim ? m_buf : m_head;
    while (node) {
        block_t *next = node->next;
        free(node->data);
        delete node;
        node = next;
    }
}

block_t *ReversedReader::next(block_t *block) {
    block_t *next = block ? block->next : nullptr;
    if (next == nullptr) {
        next = fetch_block();
        if (next) {
            if (block) {
                block->next = next;
            }
            if (!m_optim) {
                next->prev = block;
            }
        }
    }
    if (m_optim && block) {
        free(block->data);
        delete block;
    }
    return next;
}

block_t* ReversedReader::fetch_block() {
    uint8_t *buf = (uint8_t *)malloc(m_block_size);
    if (buf == nullptr)
        throw std::bad_alloc();
    size_t n = m_read_backward_func(buf, m_block_size, m_block_size);
    if (n == 0) {
        free(buf);
        return nullptr;
    }
    block_t *block = new block_t;
    block->data = buf;
    block->next = nullptr;
    block->prev = nullptr;
    size_t skip = m_block_size - n;
    block->data_view = buf + skip;
    block->size = n;
    return block;
}

    // read single char from backward, -1 for EOF
int ReversedReader::read() {
    while (m_buf) {
        if (m_bufptr > 0) {
            return m_buf->data_view[--m_bufptr];
        } else {
            m_buf = next(m_buf);
            if (m_buf) {
                m_bufptr = m_buf->size;
            } else {
                m_bufptr = 0;
            }
        }
    }
    return -1;
}

// return actual bytes read, 0 for EOF
size_t ReversedReader::read(uint8_t* buf, size_t end, size_t len) {
    // don't call read(), read by block for performance
    size_t total = 0;
    while (m_buf && len > 0) {
        size_t toRead = m_bufptr;
        if (toRead > len) toRead = len;
        if (toRead > 0) {
            memcpy(buf + end - toRead, m_buf->data_view + m_bufptr - toRead, toRead);
            m_bufptr -= toRead;
            end -= toRead;
            len -= toRead;
            total += toRead;
        } else {
            m_buf = next(m_buf);
            if (m_buf) {
                m_bufptr = m_buf->size;
            } else {
                m_bufptr = 0;
            }
        }
    }
    return total;
}

bool ReversedReader::reject() {
    return reject(1);
}

bool ReversedReader::reject(size_t n) {
    block_t *rewind = m_buf;
    size_t rewindptr = m_bufptr;

    if (!rewind && m_head) {
        block_t *tail = m_head;
        while (tail->next) {
            tail = tail->next;
        }
        rewind = tail;
        rewindptr = 0;
    }

    while (n && rewind) {
        assert(rewindptr <= rewind->size);
        size_t toReject = rewind->size - rewindptr;
        if (toReject > n)
            toReject = n;

        if (toReject > 0) {
            rewindptr += toReject;
            n -= toReject;
        } else {
            rewind = rewind->prev;
            if (rewind)
                rewindptr = 0;
        }
    }
    if (rewind) {
        m_buf = rewind;
        m_bufptr = rewindptr;
        return true;
    } else {
        return false;
    }
}

// since this is reversed reader, readAhead is actually look backward
int ReversedReader::lookAhead(uint8_t* buf, size_t end, size_t len) {
    int cb = read(buf, end, len);
    reject(cb);
    return cb;
}

// since this is reversed reader, lookAhead is actually look backward
int ReversedReader::lookAhead() {
    int ch = read();
    if (ch != -1)
        reject();
    return ch;
}

// return line (including EOL), empty string for EOF
std::string ReversedReader::readRawLine() {
    std::string line;
    int column = 0;
    while (true) {
        int b = read();
        if (b == -1) break;
        if (b == '\n') {
            // @stupidclaude: FOLLOWING IS CORRECT, DONT MODIFY
            if (column == 0) {
                // Newline at start (column 0) is the EOL of current line, must be included.
            } else {
                // otherwise, it's the EOL of the previous line, must be rejected
                // so the read() is actually a lookahead.
                reject();
                break;
            }
        }
        line.push_back(b);
        column++;
    }
    std::reverse(line.begin(), line.end());
    return line;
}

std::vector<std::string> ReversedReader::readRawLines(int maxLines) {
    std::vector<std::string> lines;
    int lineCount = 0;
    while (maxLines == -1 || lineCount < maxLines) {
        std::string line = readRawLine();
        if (line.empty()) break;
        lines.push_back(std::move(line));
        lineCount++;
    }
    return lines;
}

std::string ReversedReader::readLine(std::string_view encoding) {
    std::string raw = readRawLine();
    if (encoding == "UTF-8" || encoding.empty()) {
        return raw;
    } else {
        // decode raw using encoding and encode in utf-8 
        auto unicode = unicode::fromEncoding(raw, encoding);
        auto utf8 = unicode::convertToUTF8(unicode);
        return utf8;
    }
}

std::vector<std::string> ReversedReader::readLines(std::string_view encoding, int maxLines) {
    if (encoding == "UTF-8" || encoding.empty())
        return readRawLines(maxLines);

    std::vector<std::string> lines;
    int lineCount = 0;
    while (maxLines == -1 || lineCount < maxLines) {
        std::string rawLine = readRawLine();
        if (rawLine.empty()) break; // EOF
        auto unicode = unicode::fromEncoding(rawLine, encoding);
        auto utf8 = unicode::convertToUTF8(unicode);
        lines.push_back(std::move(utf8));
        lineCount++;
    }
    return lines;
}
