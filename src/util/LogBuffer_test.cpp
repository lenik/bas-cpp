#include "LogBuffer.hpp"

#include <cassert>
#include <iostream>

static void test_add_get_clear() {
    LogBuffer& lb = LogBuffer::instance();
    lb.clear();

    lb.addLog("INFO", "message one", "green");
    lb.addLog("WARN", "message two", "yellow");
    assert(lb.count() == 2);

    auto logs = lb.getLogs();
    assert(logs.size() == 2);
    assert(logs[0].level == "INFO");
    assert(logs[0].message == "message one");
    assert(logs[0].color == "green");
    assert(logs[1].level == "WARN");
    assert(logs[1].message == "message two");

    assert(lb.getLogText(0) == "message one");
    assert(lb.getLogLevel(0) == "INFO");
    assert(lb.getLogColor(0) == "green");
    assert(lb.getLogText(1) == "message two");
    assert(lb.getLogText(-1).empty());
    assert(lb.getLogText(99).empty());

    lb.clear();
    assert(lb.count() == 0);
    assert(lb.getLogs().empty());
}

static void test_clearLogs() {
    LogBuffer& lb = LogBuffer::instance();
    lb.addLog("X", "y", "z");
    assert(lb.count() == 1);
    lb.clearLogs();
    assert(lb.count() == 0);
}

int main() {
    test_add_get_clear();
    test_clearLogs();
    std::cout << "All LogBuffer tests passed.\n";
    return 0;
}
