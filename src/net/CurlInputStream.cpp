#include "CurlInputStream.hpp"

#include <cstring>
#include <ios>
#include <stdexcept>

#include <curl/curl.h>

namespace {

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* vec = static_cast<std::vector<uint8_t>*>(userdata);
    size_t total = size * nmemb;
    vec->insert(vec->end(), reinterpret_cast<uint8_t*>(ptr), reinterpret_cast<uint8_t*>(ptr) + total);
    return total;
}

} // namespace

CurlInputStream::CurlInputStream(const std::string& url)
    : m_url(url)
    , m_position(0)
    , m_closed(false)
    , m_ok(false)
    , m_responseCode(0)
{
    perform();
}

CurlInputStream::~CurlInputStream() {
    close();
}

void CurlInputStream::perform() {
    static bool curlInited = false;
    if (!curlInited) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curlInited = true;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        m_ok = false;
        return;
    }
    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &m_data);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_responseCode);
        m_ok = (m_responseCode >= 200 && m_responseCode < 300);
    }
    curl_easy_cleanup(curl);
}

int CurlInputStream::read() {
    if (m_closed || !m_ok || m_position >= m_data.size()) return -1;
    return static_cast<int>(m_data[m_position++]);
}

size_t CurlInputStream::read(uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !buf || !m_ok) return 0;
    size_t remaining = m_data.size() - m_position;
    if (remaining == 0) return 0;
    size_t n = (len < remaining) ? len : remaining;
    std::memcpy(buf + off, m_data.data() + m_position, n);
    m_position += n;
    return n;
}

void CurlInputStream::close() {
    m_closed = true;
}

int64_t CurlInputStream::position() const {
    return m_position;
}

bool CurlInputStream::seek(int64_t offset, std::ios::seekdir dir) {
    if (m_closed) return false;
    int64_t pos = 0;
    switch (dir) {
    case std::ios::beg:
        pos = offset;
        break;
    case std::ios::cur:
        pos = static_cast<int64_t>(m_position) + offset;
        break;
    case std::ios::end:
        pos = static_cast<int64_t>(m_data.size()) + offset;
        break;
    default:
        return false;
    }
    if (pos < 0 || static_cast<size_t>(pos) > m_data.size()) return false;
    m_position = static_cast<size_t>(pos);
    return true;
}
