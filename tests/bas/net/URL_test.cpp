// Unit tests for URL (URL-specific APIs; URI parsing covered by URI_test)

#include "URL.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_empty_and_spec() {
    URL u;
    assert(u.empty());
}

static void test_construct_from_spec() {
    URL u("https://host/path");
    assert(!u.empty());
    assert(u.getScheme() == "https");
    assert(u.spec() == "https://host/path");
}

static void test_zipEntry() {
    URL u = URL::zipEntry("/tmp/archive.zip", "dir/file.txt");
    assert(u.getScheme() == "zip");
    assert(u.spec().find("zip:") != std::string::npos);
    assert(u.spec().find("!/") != std::string::npos);
    assert(u.spec().find("dir/file.txt") != std::string::npos);
}

static void test_local_getPath() {
    URL u = URL::local("/home/user/file");
    assert(u.getScheme() == "file");
    assert(u.getPath() == "/home/user/file");
}

static void test_https_static() {
    URL u = URL::https("example.com", 443, "/api", "k=v", "frag");
    assert(u.getScheme() == "https");
    assert(u.spec().find("example.com") != std::string::npos);
    assert(u.spec().find("/api") != std::string::npos);
}

static void test_toURI_roundtrip() {
    URL u("http://x/y");
    assert(u.toURI().spec() == u.spec());
    assert(u.toURL().spec() == u.spec());
}

int main() {
    test_empty_and_spec();
    test_construct_from_spec();
    test_zipEntry();
    test_local_getPath();
    test_https_static();
    test_toURI_roundtrip();
    std::cout << "All URL tests passed.\n";
    return 0;
}
