// Unit tests for script/json.hpp (boost::json helpers).
// Build: requires linking Boost.JSON (e.g. meson or add third_party/boost_json src to tests.mf).

#include "json.hpp"

#include <cassert>
#include <iostream>

static void test_get_bool() {
    boost::json::object obj;
    obj["on"] = true;
    obj["off"] = false;
    assert(boost::json::get_bool(obj, "on", false) == true);
    assert(boost::json::get_bool(obj, "off", true) == false);
    assert(boost::json::get_bool(obj, "missing", true) == true);
    assert(boost::json::get_bool(obj, "missing", false) == false);
}

static void test_get_int() {
    boost::json::object obj;
    obj["x"] = 42;
    obj["y"] = -1;
    assert(boost::json::get_int(obj, "x", 0) == 42);
    assert(boost::json::get_int(obj, "y", 0) == -1);
    assert(boost::json::get_int(obj, "missing", 99) == 99);
}

static void test_get_string() {
    boost::json::object obj;
    obj["s"] = "hello";
    assert(boost::json::get_string(obj, "s", "") == "hello");
    assert(boost::json::get_string(obj, "missing", "default") == "default");
}

static void test_get_int64_get_double() {
    boost::json::object obj;
    obj["i"] = static_cast<int64_t>(1000000000000);
    obj["d"] = 3.14;
    assert(boost::json::get_int64(obj, "i", 0) == 1000000000000);
    assert(boost::json::get_double(obj, "d", 0) >= 3.13 && boost::json::get_double(obj, "d", 0) <= 3.15);
}

static void test_locate_missing_segment_no_crash() {
    boost::json::value root = boost::json::parse(R"({
        "mime-type.text_html": {"openwith": {"Browser": "omnishell.Browser"}}
    })");
    // Registry maps dots to slashes; a flat dotted key must not crash traversal.
    auto loc = bas::json::locate_const(root, "mime-type/text_html");
    assert(loc.node == nullptr);
    assert(loc.is_null());
}

static void test_locate_nested_path() {
    boost::json::value root = boost::json::parse(R"({"mime-type": {"text_html": {"x": 1}}})");
    auto loc = bas::json::locate_const(root, "mime-type/text_html");
    assert(loc.node != nullptr);
    assert(loc.node->is_object());
    assert(loc.node->as_object().at("x").as_int64() == 1);
}

int main() {
    test_get_bool();
    test_get_int();
    test_get_string();
    test_get_int64_get_double();
    test_locate_missing_segment_no_crash();
    test_locate_nested_path();
    std::cout << "All json tests passed.\n";
    return 0;
}
