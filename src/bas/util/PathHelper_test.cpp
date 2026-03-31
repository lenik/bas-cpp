#include "PathHelper.hpp"

#include <cassert>
#include <iostream>
#include <vector>

static void test_normalizePath() {
    assert(PathHelper::normalizePath("") == "/");
    assert(PathHelper::normalizePath("/") == "/");
    assert(PathHelper::normalizePath("a/b") == "/a/b");
    assert(PathHelper::normalizePath("/a/b/") == "/a/b");
    assert(PathHelper::normalizePath("a//b") == "/a/b");
    assert(PathHelper::normalizePath("a\\b") == "/a/b");
}

static void test_joinPaths() {
    assert(PathHelper::joinPaths("", "b") == "/b");
    assert(PathHelper::joinPaths("/a", "b") == "/a/b");
    assert(PathHelper::joinPaths("/a/", "b") == "/a/b");
    assert(PathHelper::joinPaths("/a", "/b") == "/a/b");
}

static void test_getParentPath() {
    assert(PathHelper::getParentPath("/") == "/");
    assert(PathHelper::getParentPath("/a") == "/");
    assert(PathHelper::getParentPath("/a/b") == "/a");
    assert(PathHelper::getParentPath("/a/b/c") == "/a/b");
}

static void test_getFileName() {
    assert(PathHelper::getFileName("/") == "");
    assert(PathHelper::getFileName("/a") == "a");
    assert(PathHelper::getFileName("/a/b.txt") == "b.txt");
}

static void test_getFileExtension_getBaseName() {
    assert(PathHelper::getFileExtension("/a/b.txt") == "txt");
    assert(PathHelper::getBaseName("/a/b.txt") == "b");
    assert(PathHelper::getFileExtension("/a/b") == "");
    assert(PathHelper::getBaseName("/a/b") == "b");
    assert(PathHelper::getFileExtension("/a/.hidden") == "");
    assert(PathHelper::getBaseName("/a/.hidden") == ".hidden");
}

static void test_isAbsolutePath() {
    assert(PathHelper::isAbsolutePath("/"));
    assert(PathHelper::isAbsolutePath("/a"));
    assert(!PathHelper::isAbsolutePath(""));
    assert(!PathHelper::isAbsolutePath("a"));
}

static void test_isValidPath() {
    assert(PathHelper::isValidPath("/a"));
    assert(!PathHelper::isValidPath(""));
    assert(!PathHelper::isValidPath("a..b"));
}

static void test_toUnixPath() {
    assert(PathHelper::toUnixPath("a/b") == "a/b");
    assert(PathHelper::toUnixPath("a\\b") == "a/b");
}

static void test_splitPath_buildPath() {
    std::vector<std::string> parts = PathHelper::splitPath("/a/b/c");
    assert(parts.size() == 3);
    assert(parts[0] == "a" && parts[1] == "b" && parts[2] == "c");
    assert(PathHelper::splitPath("/").empty());
    assert(PathHelper::buildPath(parts) == "/a/b/c");
    assert(PathHelper::buildPath({}) == "/");
}

int main() {
    test_normalizePath();
    test_joinPaths();
    test_getParentPath();
    test_getFileName();
    test_getFileExtension_getBaseName();
    test_isAbsolutePath();
    test_isValidPath();
    test_toUnixPath();
    test_splitPath_buildPath();
    std::cout << "All PathHelper tests passed.\n";
    return 0;
}
