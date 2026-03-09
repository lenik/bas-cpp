// Unit tests for ProtocolRegistry.
// Build: meson (needs ProtocolRegistry + all registered protocols and net + io chain).

#include "ProtocolRegistry.hpp"
#include "URL.hpp"

#include <cassert>
#include <iostream>

static void test_getProtocol_file() {
    auto& reg = ProtocolRegistry::instance();
    NetProtocol* p = reg.getProtocol("file");
    assert(p != nullptr);
    assert(p->getProtocolName() == "file");
}

static void test_getProtocol_data() {
    auto& reg = ProtocolRegistry::instance();
    NetProtocol* p = reg.getProtocol("data");
    assert(p != nullptr);
    assert(p->getProtocolName() == "data");
}

static void test_getProtocol_http_https_ftp_ftps() {
    auto& reg = ProtocolRegistry::instance();
    assert(reg.getProtocol("http") != nullptr && reg.getProtocol("http")->getProtocolName() == "http");
    assert(reg.getProtocol("https") != nullptr && reg.getProtocol("https")->getProtocolName() == "https");
    assert(reg.getProtocol("ftp") != nullptr && reg.getProtocol("ftp")->getProtocolName() == "ftp");
    assert(reg.getProtocol("ftps") != nullptr && reg.getProtocol("ftps")->getProtocolName() == "ftps");
}

static void test_getProtocolFor_data_url() {
    auto& reg = ProtocolRegistry::instance();
    URL url("data:text/plain,hello");
    NetProtocol* p = reg.getProtocolFor(url);
    assert(p != nullptr);
    assert(p->getProtocolName() == "data");
}

static void test_getProtocol_unknown_returns_null() {
    auto& reg = ProtocolRegistry::instance();
    assert(reg.getProtocol("unknown") == nullptr);
    assert(reg.getProtocol("") == nullptr);
}

int main() {
    test_getProtocol_file();
    test_getProtocol_data();
    test_getProtocol_http_https_ftp_ftps();
    test_getProtocolFor_data_url();
    test_getProtocol_unknown_returns_null();
    std::cout << "All ProtocolRegistry tests passed.\n";
    return 0;
}
