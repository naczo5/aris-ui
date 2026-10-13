// aris-ui — tiny zero-dependency test harness (no GL, no external deps).
// TEST(name) registers a test function; CHECK/CHECK_* record failures with
// file:line and the process exits non-zero if anything failed.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "ui/core/math.hpp"

namespace aris_test {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int& Failures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { Registry().push_back({name, fn}); }
};

inline void Report(const char* file, int line, const std::string& msg) {
    ++Failures();
    std::fprintf(stderr, "FAIL %s:%d  %s\n", file, line, msg.c_str());
}

template <typename T>
inline std::string ToStr(const T& v) { return std::to_string(v); }
inline std::string ToStr(const ui::Vec2& v) {
    return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
}
inline std::string ToStr(const ui::Vec4& v) {
    return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " +
           std::to_string(v.z) + ", " + std::to_string(v.w) + ")";
}
inline std::string ToStr(const ui::Rect& r) {
    return "(" + std::to_string(r.x) + ", " + std::to_string(r.y) + ", " +
           std::to_string(r.w) + " x " + std::to_string(r.h) + ")";
}
inline std::string ToStr(const ui::Color& c) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    return buf;
}

template <typename A, typename B>
inline std::string Fmt(const A& a, const B& b) {
    return "(" + ToStr(a) + " vs " + ToStr(b) + ")";
}

} // namespace aris_test

#define ARIS_CONCAT2(a, b) a##b
#define ARIS_CONCAT(a, b) ARIS_CONCAT2(a, b)

#define TEST(name)                                                        \
    static void ARIS_CONCAT(test_fn_, name)();                            \
    static ::aris_test::Registrar ARIS_CONCAT(test_reg_, name)(#name,     \
        &ARIS_CONCAT(test_fn_, name));                                    \
    static void ARIS_CONCAT(test_fn_, name)()

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond))                                                      \
            ::aris_test::Report(__FILE__, __LINE__, "CHECK failed: " #cond); \
    } while (0)

#define CHECK_EQ(a, b)                                                    \
    do {                                                                  \
        auto _va = (a);                                                   \
        auto _vb = (b);                                                   \
        if (!(_va == _vb))                                                \
            ::aris_test::Report(__FILE__, __LINE__,                       \
                std::string("CHECK_EQ failed: ") + #a " == " #b " " +     \
                ::aris_test::Fmt(_va, _vb));                              \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                             \
    do {                                                                  \
        double _va = (a), _vb = (b), _ve = (eps);                          \
        if (!(std::fabs(_va - _vb) <= _ve))                               \
            ::aris_test::Report(__FILE__, __LINE__,                       \
                std::string("CHECK_NEAR failed: ") + #a " ~ " #b " " +    \
                ::aris_test::Fmt(_va, _vb));                              \
    } while (0)

#define CHECK_FALSE(cond) CHECK(!(cond))

inline int RunAllTests() {
    int failedBefore = 0;
    for (const auto& t : ::aris_test::Registry()) {
        failedBefore = ::aris_test::Failures();
        t.fn();
        if (::aris_test::Failures() == failedBefore)
            std::printf("ok   %s\n", t.name);
        else
            std::printf("FAIL %s\n", t.name);
    }
    const int f = ::aris_test::Failures();
    std::printf("%zu tests, %d failure(s)\n", ::aris_test::Registry().size(), f);
    return f ? 1 : 0;
}