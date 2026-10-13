#pragma once
// aris-ui — easing / interpolation engine
#include <cmath>
#include "ui/core/math.hpp"

namespace ui {

inline float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Vec2 Lerp(const Vec2& a, const Vec2& b, float t) { return a + (b - a) * t; }

// Frame-rate independent exponential approach (used for widget transitions).
// `speed` is roughly "how fast it converges" — 10 is smooth, 25 is snappy.
inline float ExpSmooth(float current, float target, float dt, float speed) {
    if (speed <= 0.f || dt <= 0.f) return current;
    const float t = Clamp01(1.f - std::exp(-speed * dt));
    return Lerp(current, target, t);
}

inline float EaseOutCubic(float t) {
    t = Clamp01(t);
    const float u = 1.f - t;
    return 1.f - u * u * u;
}

inline float EaseInOutCubic(float t) {
    t = Clamp01(t);
    return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) * 0.5f;
}

inline float EaseOutBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.f;
    t = Clamp01(t);
    const float u = t - 1.f;
    return 1.f + c3 * u * u * u + c1 * u * u;
}

// CSS-style cubic-bezier easing: solve bezier-x(t) == x (Newton-Raphson),
// then return bezier-y(t). control points p1/p2 must have x in [0,1].
inline float CubicBezierEase(float x, Vec2 p1, Vec2 p2) {
    x = Clamp01(x);
    const auto bez = [](float t, float a, float b, float c) {
        const float s = 1.f - t;
        return s * s * s * a + 3.f * s * s * t * b + 3.f * s * t * t * c + t * t * t;
    };
    // cubic-bezier(0, 0, p1.x, p1.y, p2.x, p2.y, 1, 1)
    float t = x; // good initial guess for monotone curves
    for (int i = 0; i < 8; ++i) {
        const float currentX = bez(t, 0.f, p1.x, p2.x) + 0.f; // x0 = 0
        const float currentY = bez(t, 0.f, p1.y, p2.y);
        (void)currentY;
        const float dx = currentX - x;
        if (std::fabs(dx) < 1e-5f) break;
        // derivative of x(t)
        const float s = 1.f - t;
        const float dX = 3.f * s * s * p1.x + 6.f * s * t * (p2.x - p1.x) + 3.f * t * t * (1.f - p2.x);
        if (std::fabs(dX) < 1e-6f) break;
        t -= dx / dX;
        t = Clamp01(t);
    }
    return bez(t, 0.f, p1.y, p2.y);
}

} // namespace ui