#pragma once

#include <array>
#include <string>

/// This macro converts the input X macro into a quoted string by double evaluating it.
#define CAPTAINS_LOG_STRINGIFY(X) CAPTAINS_LOG_STRINGIFY2(X)
#define CAPTAINS_LOG_STRINGIFY2(X) #X

namespace CAP {

/// @brief Helper that lets you make inline handlers for variants
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

/// @brief Thin wrapper on std::array that makes Size statically accessible for templates
/// @tparam T The underlying type in the array
/// @tparam N The size of the array
template <class T, size_t N>
struct ArrayN {
    ArrayN(std::array<T, N> array) : v(std::move(array)) {}

    ArrayN() = default;

    const T& operator[](int index) const { return v[index]; }

    T& operator[](int index) { return v[index]; }

    std::array<T, N> v;
    constexpr static size_t Size = N;
};

/// @brief Helper method that calls std::to_string on the argument if needed.
/// @tparam T The argument's type
/// @param t The argument's instance
/// @return The std::string produced by optionally calling std::to_string on the argument.
template <class T>
std::string stringify(T&& t) {
    // if std::string or literal string, just return it (with implicit conversion for literal
    // string)
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> ||
                  std::is_same_v<std::decay_t<T>, const char*>) {
        return t;
    } else {
        return std::to_string(t);
    }
}

/// @brief Helper that accepts variables arguments of anything that's a string or can be converted
/// with std::to_string.
/// @param ...args variable args that are strings or can be converted to std::to_string
/// @return std::string produced from concatenating all the arguments.
template <class... Args>
std::string string(Args... args) {
    return (stringify(args) + ...);
}

}  // namespace CAP

// Note, prefer to use CAP::string(...) instead. eg. CAP::string("VarBase", 1);
#define CAP_LOG_FSTRING_HELPER(...)                                                      \
    []() -> std::string {                                                                \
        size_t needed = snprintf(NULL, 0, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__)) + 1; \
        std::string buffer;                                                              \
        buffer.resize(needed);                                                           \
        snprintf(&buffer[0], needed, FIRST(__VA_ARGS__) " " REST(__VA_ARGS__));          \
        delete[] buffer;                                                                 \
        return buffer;                                                                   \
    }();

/// This section is a suite of macros that allow us to build printf-like caplog macros
/// that still understand something about specific arguments.
///////
// https://stackoverflow.com/questions/8487986/file-macro-shows-full-path
#define __CAP_FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

///////
// https://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick/11172679#11172679
/* expands to the first argument */
#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first

/*
 * if there's only one argument, expands to nothing.  if there is more
 * than one argument, expands to a comma followed by everything but
 * the first argument.  only supports up to 9 arguments but can be
 * trivially expanded.
 */
#define REST(...) REST_HELPER(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_HELPER(qty, ...) REST_HELPER2(qty, __VA_ARGS__)
#define REST_HELPER2(qty, ...) REST_HELPER_##qty(__VA_ARGS__)
#define REST_HELPER_ONE(first)
#define REST_HELPER_TWOORMORE(first, ...) , __VA_ARGS__
#define NUM(...)                                                                               \
    SELECT_20TH(__VA_ARGS__, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, \
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,   \
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, ONE, throwaway)
#define SELECT_20TH(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, \
                    a18, a19, a20, ...)                                                         \
    a20
///////

#ifdef SHOW_THREAD_ID
#define INSERT_THREAD_ID << std::this_thread::get_id()
#else
#define INSERT_THREAD_ID ""
#endif
