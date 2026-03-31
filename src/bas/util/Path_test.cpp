#include "Path.hpp"

#include <cassert>
#include <iostream>
#include <string_view>

static void test_simple_path() {
    Path p("a/b/c.txt");
    assert(p.str() == "a/b/c.txt");
    assert(p.dir() == "a/b");
    assert(p.dir_() == "a/b/");
    assert(p.base() == "c.txt");
    assert(p.name() == "c");
    assert(p.extension(false) == "txt");
    assert(p.extension(true) == ".txt");
    assert(!p.isAbsolute());
}

static void test_absolute_path() {
    Path p("/usr/local/bin");
    assert(p.isAbsolute());
    assert(p.dir() == "/usr/local");
    assert(p.base() == "bin");
    assert(p.name() == "bin");
    assert(p.extension() == "");
}

static void test_trailing_slash() {
    Path p("foo/bar/");
    assert(p.str() == "foo/bar/");
    assert(p.dir() == "foo/bar");
    assert(p.base() == "");  // trailing slash => empty base
}

static void test_no_extension() {
    Path p("a/b/README");
    assert(p.name() == "README");
    assert(p.extension() == "");
    assert(p.extension(true) == "");
}

static void test_hidden_file() {
    Path p("a/.hidden");
    assert(p.base() == ".hidden");
    assert(p.name() == ".hidden");  // dot at start => no extension
    assert(p.extension() == "");
}

static void test_multiple_dots() {
    Path p("a/file.tar.gz");
    assert(p.name() == "file.tar");
    assert(p.extension(false) == "gz");
    assert(p.extension(true) == ".gz");
}

static void test_compare() {
    assert(Path("a") < Path("b"));
    assert(Path("a/b") < Path("a/c"));
    assert(Path("a") < Path("a/b"));   // directory before file in same path
    assert(Path("a") == Path("a"));
    assert(Path("/x") < Path("x"));  // '/' < 'x' in pathCompareC
}

static void test_getParent() {
    assert(Path("a/b/c").getParent().str() == "a/b");
    assert(Path("/a/b").getParent().str() == "/a");
}

static void test_toAbsolute_toRelative() {
    Path p("foo/bar");
    assert(!p.isAbsolute());
    Path abs = p.toAbsolute();
    assert(abs.isAbsolute());
    assert(abs.str() == "/foo/bar");
    Path rel = abs.toRelative();
    assert(!rel.isAbsolute());
    assert(rel.str() == "foo/bar");
}

static void test_toDir_toFile() {
    Path p("a/b");
    Path dir = p.toDir();
    assert(dir.str() == "a/b/");
    assert(dir.toFile().str() == "a/b");
    assert(Path("/").toFile().str() == "/");
}

static void test_join() {
    Path base("a/b");
    Path rel("c/d");
    assert(base.join(rel).str() == "a/b/c/d");
    Path abs("/x");
    assert(base.join(abs).str() == "/x");
}

static void test_dir_base_name_extension_setters() {
    Path p("old/dir/file.txt");
    assert(p.dir("/new").str() == "/new/file.txt");
    assert(p.base("other.txt").str() == "old/dir/other.txt");
    assert(p.name("name").str() == "old/dir/name.txt");
    assert(p.extension(std::string_view("log")).str() == "old/dir/file.log");
    assert(p.extension(std::string_view(".log")).str() == "old/dir/file.log");
}

static void test_constructor_dir_base() {
    Path p("dir", "base");
    assert(p.str() == "dir/base");
    Path p2("dir/", "base");
    assert(p2.str() == "dir/base");
    Path p3("", "base");
    assert(p3.str() == "base");
    Path p4("dir", "/abs");
    assert(p4.str() == "/abs");
}

static void test_href() {
    Path base("a/b/");
    Path rel("c");
    assert(base.href(rel).str() == "a/b/c");
    Path abs("/x");
    assert(base.href(abs).str() == "/x");
}

int main() {
    test_simple_path();
    test_absolute_path();
    test_trailing_slash();
    test_no_extension();
    test_hidden_file();
    test_multiple_dots();
    test_compare();
    test_getParent();
    test_toAbsolute_toRelative();
    test_toDir_toFile();
    test_join();
    test_dir_base_name_extension_setters();
    test_constructor_dir_base();
    test_href();
    std::cout << "All Path tests passed.\n";
    return 0;
}
