// Unit tests for URI and URL (parsing and static builders)

#include "URI.hpp"
#include "URL.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_parse_http() {
    URI u("http://example.com/path?q=1#frag");
    assert(u.scheme() && *u.scheme() == "http");
    assert(u.host() && *u.host() == "example.com");
    assert(u.path() && *u.path() == "/path");
    assert(u.query() && *u.query() == "q=1");
    assert(u.fragment() && *u.fragment() == "frag");
    assert(!u.empty());
}

static void test_parse_authority_user_pass() {
    URI u("https://user:pass@host:443/");
    assert(u.scheme() && *u.scheme() == "https");
    assert(u.user() && *u.user() == "user");
    assert(u.password() && *u.password() == "pass");
    assert(u.host() && *u.host() == "host");
    assert(u.port() && *u.port() == 443);
}

static void test_spec_roundtrip() {
    std::string s = "http://a.b/c?d#e";
    URI u(s);
    assert(u.spec() == s);
}

static void test_uri_http_static() {
    URI u = URI::http("host", 80, "/p");
    assert(*u.scheme() == "http");
    assert(*u.host() == "host");
    assert(!u.port());  // 80 is default, omitted
    assert(*u.path() == "/p");
}

static void test_uri_https_with_port() {
    URI u = URI::https("h", 8443, "/x", "a=b", "f");
    assert(*u.scheme() == "https");
    assert(*u.host() == "h");
    assert(u.port() && *u.port() == 8443);
    assert(*u.path() == "/x");
    assert(*u.query() == "a=b");
    assert(*u.fragment() == "f");
}

static void test_uri_file() {
    URI u = URI::file("/tmp/foo");
    assert(*u.scheme() == "file");
    assert(u.path() && *u.path() == "/tmp/foo");
    assert(u.dir() && *u.dir() == "/tmp");
    assert(u.base() && *u.base() == "foo");
}

static void test_toURL() {
    URI u("http://x/y");
    URL url = u.toURL();
    assert(!url.empty());
    assert(url.spec() == "http://x/y");
    assert(url.getScheme() == "http");
}

static void test_url_local() {
    URL url = URL::local("/home/a");
    assert(url.getScheme() == "file");
    assert(url.getPath() == "/home/a");
}

static void test_url_http_static() {
    URL url = URL::http("h", 0, "/p");  // port 0 = default 80
    assert(url.spec().find("http") != std::string::npos);
    assert(url.getScheme() == "http");
}

int main() {
    test_parse_http();
    test_parse_authority_user_pass();
    test_spec_roundtrip();
    test_uri_http_static();
    test_uri_https_with_port();
    test_uri_file();
    test_toURL();
    test_url_local();
    test_url_http_static();
    std::cout << "All URI/URL tests passed.\n";
    return 0;
}
