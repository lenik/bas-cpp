#ifndef LOGBUFFER_H
#define LOGBUFFER_H

#include <ctime>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

struct LogEntry {
    std::time_t timestamp;
    std::string level;
    std::string message;
    std::string color;
    
    LogEntry() : timestamp(0) {}
    LogEntry(std::string_view lvl, std::string_view msg, std::string_view col)
        : timestamp(std::time(nullptr)), level(lvl), message(msg), color(col) {}
};

class LogBuffer {
public:
    static LogBuffer& instance();
    
    void addLog(std::string_view level, std::string_view message, std::string_view color);
    std::vector<LogEntry> getLogs() const;
    void clear();
    int count() const;
    
    std::string getLogText(int index) const;
    std::string getLogLevel(int index) const;
    std::string getLogColor(int index) const;
    std::string getLogTimestamp(int index) const;
    void clearLogs();
    
    // Callback setter for event handling
    void setLogAddedCallback(std::function<void(std::string_view, std::string_view, std::string_view)> callback) {
        m_logAddedCallback = callback;
    }
    
private:
    LogBuffer();
    ~LogBuffer() = default;
    LogBuffer(const LogBuffer&) = delete;
    LogBuffer& operator=(const LogBuffer&) = delete;
    
    mutable std::mutex m_mutex;
    std::vector<LogEntry> m_logs;
    static const int MAX_LOGS = 10000;
    
    // Callback for event handling
    std::function<void(std::string_view, std::string_view, std::string_view)> m_logAddedCallback;
};

#endif // LOGBUFFER_H
