#include "PrintStream.hpp"

PrintStream::PrintStream(std::unique_ptr<OutputStream> out, std::string_view charset)
    : m_writer(std::move(out), charset)
{
}

PrintStream::~PrintStream() {
    m_writer.close();
}

bool PrintStream::writeChar(char32_t codePoint) {
    return m_writer.writeChar(codePoint);
}

bool PrintStream::write(std::string_view data) {
    return m_writer.write(data);
}

bool PrintStream::writeln() {
    return m_writer.writeln();
}

bool PrintStream::writeln(std::string_view data) {
    return m_writer.writeln(data);
}

void PrintStream::flush() {
    m_writer.flush();
}

void PrintStream::close() {
    m_writer.close();
}
