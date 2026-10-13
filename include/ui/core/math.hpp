#pragma once
// aris-ui — core math primitives (platform-agnostic, no GL/platform deps)
#include <cstdint>
#include <cmath>

namespace ui {

struct Vec2 {
    float x = 0.f, y = 0.f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }

    constexpr bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const Vec2& o) const { return !(*this == o); }

    float Length() const { return std::sqrt(x * x + y * y); }
    float Dot(const Vec2& o) const { return x * o.x + y * o.y; }
};

struct Vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
    constexpr Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vec4(float v) : x(v), y(v), z(v), w(v) {}

    constexpr float operator[](int i) const {
        return i == 0 ? x : (i == 1 ? y : (i == 2 ? z : w));
    }
    constexpr bool operator==(const Vec4& o) const {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
    constexpr bool operator!=(const Vec4& o) const { return !(*this == o); }
};

struct Rect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;

    constexpr Rect() = default;
    constexpr Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}

    constexpr bool Contains(Vec2 p) const {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }
    constexpr bool Valid() const { return w > 0.f && h > 0.f; }
    constexpr Vec2 Pos() const { return {x, y}; }
    constexpr Vec2 Size() const { return {w, h}; }
    constexpr Vec2 Center() const { return {x + w * 0.5f, y + h * 0.5f}; }
    constexpr Vec2 Max() const { return {x + w, y + h}; }

    constexpr Rect Inset(float t) const { return {x + t, y + t, w - 2 * t, h - 2 * t}; }
    constexpr Rect Inset(float horizontal, float vertical) const {
        return {x + horizontal, y + vertical, w - 2 * horizontal, h - 2 * vertical};
    }
    constexpr Rect Offset(Vec2 d) const { return {x + d.x, y + d.y, w, h}; }

    constexpr bool operator==(const Rect& o) const {
        return x == o.x && y == o.y && w == o.w && h == o.h;
    }
    constexpr bool operator!=(const Rect& o) const { return !(*this == o); }
};

// 8-bit RGBA color.
struct Color {
    std::uint8_t r = 0, g = 0, b = 0, a = 0;

    constexpr Color() = default;
    constexpr Color(std::uint8_t r_, std::uint8_t g_, std::uint8_t b_, std::uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    static constexpr Color Transparent() { return Color(0, 0, 0, 0); }
    static constexpr Color White() { return Color(255, 255, 255, 255); }
    static constexpr Color Black() { return Color(0, 0, 0, 255); }

    // 0xRRGGBB (optionally 0xAARRGGBB when withAlpha provided separately)
    static constexpr Color Hex(unsigned int rgb, std::uint8_t alpha = 255) {
        return Color(static_cast<std::uint8_t>((rgb >> 16) & 0xFF),
                     static_cast<std::uint8_t>((rgb >> 8) & 0xFF),
                     static_cast<std::uint8_t>(rgb & 0xFF), alpha);
    }

    static Color FromFloats(float r_, float g_, float b_, float a_ = 1.f) {
        auto c = [](float v) { return static_cast<std::uint8_t>(v * 255.f + 0.5f); };
        return Color(c(r_), c(g_), c(b_), c(a_));
    }

    constexpr Color WithAlpha(std::uint8_t newAlpha) const {
        return Color(r, g, b, newAlpha);
    }

    Vec4 ToVec4() const {
        return Vec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
    }

    static Color Blend(const Color& from, const Color& to, float t) {
        auto ch = [&t](std::uint8_t a, std::uint8_t b) {
            return static_cast<std::uint8_t>(a + (static_cast<float>(b) - static_cast<float>(a)) * t + 0.5f);
        };
        return Color(ch(from.r, to.r), ch(from.g, to.g), ch(from.b, to.b), ch(from.a, to.a));
    }

    constexpr bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    constexpr bool operator!=(const Color& o) const { return !(*this == o); }
};

} // namespace ui