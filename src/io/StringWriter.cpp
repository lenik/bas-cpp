#include "StringWriter.hpp"

#include "../util/unicode.hpp"

bool StringWriter::writeChar(char32_t codePoint) {
    unicode::utf8EncodeCodePoint(codePoint, m_buffer);
    return true;
}

bool StringWriter::write(std::string_view data) {
    m_buffer.append(data);
    return true;
}

bool StringWriter::writeln() {
    m_buffer.push_back('\n');
    return true;
}

bool StringWriter::writeln(std::string_view data) {
    m_buffer.append(data);
    m_buffer.push_back('\n');
    return true;
}

void StringWriter::flush() {}

void StringWriter::close() {}
