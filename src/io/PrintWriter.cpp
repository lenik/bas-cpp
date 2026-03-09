#include "PrintWriter.hpp"

PrintWriter::PrintWriter(Writer* out)
    : m_out(out)
{
}

bool PrintWriter::writeChar(char32_t codePoint) {
    return m_out ? m_out->writeChar(codePoint) : false;
}

bool PrintWriter::write(std::string_view data) {
    return m_out ? m_out->write(data) : false;
}

bool PrintWriter::writeln() {
    return m_out ? m_out->writeln() : false;
}

bool PrintWriter::writeln(std::string_view data) {
    return m_out ? m_out->writeln(data) : false;
}

void PrintWriter::flush() {
    if (m_out) m_out->flush();
}

void PrintWriter::close() {
    if (m_out) m_out->close();
}
