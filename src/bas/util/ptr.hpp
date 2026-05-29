#ifndef BAS_UTIL_PTR_HPP
#define BAS_UTIL_PTR_HPP

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

template <typename> struct is_unique_ptr : std::false_type {};
template <typename U, typename D> struct is_unique_ptr<std::unique_ptr<U, D>> : std::true_type {};

template <typename> struct is_shared_ptr : std::false_type {};
template <typename U> struct is_shared_ptr<std::shared_ptr<U>> : std::true_type {};

template <typename T> //
struct VariantPtr {
    using value_type = T;

    enum class Type : std::uint8_t {
        POINTER = 0,
        UNIQUE_PTR,
        SHARED_PTR,
        REFERENCE,
    };

    std::variant<T*,                 //
                 std::unique_ptr<T>, //
                 std::shared_ptr<T>, //
                 std::reference_wrapper<T>>
        variant;

    VariantPtr(T* ptr) : variant(ptr) {}
    VariantPtr(T& ref) : variant(std::ref(ref)) {} // Must wrap ref explicitly

    // Accept a concrete temporary; not for smart pointers or abstract T.
    template <typename U, std::enable_if_t<!is_unique_ptr<std::decay_t<U>>::value &&
                                               !is_shared_ptr<std::decay_t<U>>::value &&
                                               std::is_constructible_v<std::decay_t<U>, U>,
                                           int> = 0>
    VariantPtr(U&& val) : variant(std::make_unique<std::decay_t<U>>(std::forward<U>(val))) {}

    template <typename U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
    VariantPtr(std::unique_ptr<U> ptr) : variant(std::unique_ptr<T>(std::move(ptr))) {}

    template <typename U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
    VariantPtr(std::shared_ptr<U> ptr) : variant(std::shared_ptr<T>(std::move(ptr))) {}

    inline Type type() const { return static_cast<Type>(variant.index()); }

    std::string typeName() {
        static constexpr std::array typeNames = //
            {"pointer", "unique_ptr", "shared_ptr", "reference"};
        return typeNames[variant.index()];
    }

    T* operator->() {
        switch (type()) {
        case Type::POINTER:
            return std::get<0>(variant); // Using index avoids heavy type typing
        case Type::UNIQUE_PTR:
            return std::get<1>(variant).get();
        case Type::SHARED_PTR:
            return std::get<2>(variant).get();
        case Type::REFERENCE:
            return &std::get<3>(variant).get();
        }
        throw std::bad_variant_access();
    }
    const T* operator->() const { return const_cast<VariantPtr*>(this)->operator->(); }

    T& operator*() { return *(operator->()); }
    const T& operator*() const { return *(operator->()); }
};

#endif // BAS_UTIL_PTR_HPP
