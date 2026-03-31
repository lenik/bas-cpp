#include "LogBuffer.hpp"

#include <cstring>
#include <ctime>

LogBuffer& LogBuffer::instance() {
    static LogBuffer instance;
    return instance;
}

LogBuffer::LogBuffer() = default;

void LogBuffer::addLog(std::string_view level, std::string_view message, std::string_view color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (static_cast<int>(m_logs.size()) >= MAX_LOGS) {
        m_logs.erase(m_logs.begin());
    }

    m_logs.emplace_back(level, message, color);

    if (m_logAddedCallback) {
        m_logAddedCallback(level, message, color);
    }
}

std::vector<LogEntry> LogBuffer::getLogs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_logs;
}

void LogBuffer::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logs.clear();
}

int LogBuffer::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_logs.size());
}

std::string LogBuffer::getLogText(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_logs.size())) {
        return m_logs[static_cast<size_t>(index)].message;
    }
    return {};
}

std::string LogBuffer::getLogLevel(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_logs.size())) {
        return m_logs[static_cast<size_t>(index)].level;
    }
    return {};
}

std::string LogBuffer::getLogColor(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_logs.size())) {
        return m_logs[static_cast<size_t>(index)].color;
    }
    return {};
}

std::string LogBuffer::getLogTimestamp(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index < 0 || index >= static_cast<int>(m_logs.size())) {
        return {};
    }
    std::time_t t = m_logs[static_cast<size_t>(index)].timestamp;
    std::tm* tm = std::localtime(&t);
    if (!tm) return {};
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm) == 0) {
        return {};
    }
    return std::string(buf);
}

void LogBuffer::clearLogs() {
    clear();
}
