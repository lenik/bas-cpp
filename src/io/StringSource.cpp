#include "StringSource.hpp"

#include "StringReader.hpp"
#include "U32stringReader.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <memory>
#include <string>

#include <unicode/unistr.h>

StringSource::StringSource(const std::string& data, std::string_view charset)
    : m_data(data)
    , m_charset(charset.empty() ? "UTF-8" : charset)
{
}

std::unique_ptr<Reader> StringSource::newReader() {
    return std::make_unique<StringReader>(m_data);
}

std::unique_ptr<RandomReader> StringSource::newRandomReader() {
    return std::make_unique<U32stringReader>(m_data);
}
