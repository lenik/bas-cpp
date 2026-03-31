#include "CurlOutputStream.hpp"

#include <cstring>

#include <curl/curl.h>

namespace {

struct ReadState {
    const std::vector<uint8_t>* data;
    size_t position;
};

size_t readCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* state = static_cast<ReadState*>(userdata);
    size_t total = size * nmemb;
    size_t available = state->data->size() - state->position;
    if (total > available) total = available;
    if (total > 0) {
        std::memcpy(ptr, state->data->data() + state->position, total);
        state->position += total;
    }
    return total;
}

} // namespace

CurlOutputStream::CurlOutputStream(const std::string& url, bool append)
    : m_url(url)
    , m_closed(false)
    , m_ok(false)
    , m_responseCode(0)
    , m_append(append)
{
}

CurlOutputStream::~CurlOutputStream() {
    close();
}

bool CurlOutputStream::write(int byte) {
    if (m_closed) return false;
    m_buffer.push_back(static_cast<uint8_t>(byte & 0xFF));
    return true;
}

size_t CurlOutputStream::write(const uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !buf) return 0;
    m_buffer.insert(m_buffer.end(), buf + off, buf + off + len);
    return len;
}

void CurlOutputStream::flush() {
    // No-op until close
}

void CurlOutputStream::performPut() {
    if (m_buffer.empty() && !m_append) {
        m_ok = false;
        return;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        m_ok = false;
        return;
    }
    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    if (m_append) {
        curl_easy_setopt(curl, CURLOPT_APPEND, 1L);
    }

    ReadState state{&m_buffer, 0};
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &state);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(m_buffer.size()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_responseCode);
        m_ok = (m_responseCode >= 200 && m_responseCode < 300);
    } else {
        m_ok = false;
    }
    curl_easy_cleanup(curl);
}

void CurlOutputStream::close() {
    if (m_closed) return;
    m_closed = true;
    performPut();
}
