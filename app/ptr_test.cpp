#include <bas/util/ptr.hpp>

#include <cassert>
#include <iostream>

void test_int() {
    {
        int x = 1;
        VariantPtr<int> ptr(&x);
        assert(ptr.type() == VariantPtr<int>::Type::POINTER);
        assert(*ptr == 1);
    }
    {
        int x = 1;
        VariantPtr<int> ptr(x);
        assert(ptr.type() == VariantPtr<int>::Type::REFERENCE);
        assert(*ptr == 1);
    }
    {
        std::unique_ptr<int> ptr = std::make_unique<int>(1);
        VariantPtr<int> vptr(std::move(ptr));
        assert(vptr.type() == VariantPtr<int>::Type::UNIQUE_PTR);
    }

    {
        std::shared_ptr<int> ptr = std::make_shared<int>(1);
        VariantPtr<int> vptr(std::move(ptr));
        assert(vptr.type() == VariantPtr<int>::Type::SHARED_PTR);
        assert(*vptr == 1);
    }
}

void test_string() {
    std::string s = "hello";
    {
        VariantPtr<std::string> ptr(s);
        assert(ptr.type() == VariantPtr<std::string>::Type::REFERENCE);
        assert(*ptr == "hello");
    }
    {
        VariantPtr<std::string> ptr(&s);
        assert(ptr.type() == VariantPtr<std::string>::Type::POINTER);
        assert(*ptr == "hello");
    }
    {
        VariantPtr<std::string> ptr(std::make_unique<std::string>(s));
        assert(ptr.type() == VariantPtr<std::string>::Type::UNIQUE_PTR);
        assert(*ptr == "hello");
    }
    {
        VariantPtr<std::string> ptr(std::make_shared<std::string>(s));
        assert(ptr.type() == VariantPtr<std::string>::Type::SHARED_PTR);
        assert(*ptr == "hello");
    }
}

class Foo {
  public:
    VariantPtr<std::string> ptr;
    Foo(VariantPtr<std::string> str) : ptr(std::move(str)) {}
    void print() const { std::cout << *ptr << std::endl; }
};

int main() {
    test_int();
    test_string();
    std::cout << "All tests passed.\n";

    auto str = std::string("hello");
    // VariantPtr<std::string> str_ptr(str);
    Foo foo(str);
    Foo foo2(std::string("world"));
    foo.print();
    printf("foo2 type: %s\n", foo2.ptr.typeName().c_str());
    return 0;
}