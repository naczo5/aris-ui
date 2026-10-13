#pragma once
// aris-ui — high-performance animation & physics engine for immediate-mode UI.
// Provides spring physics, multi-curve tweens, elastic/bounce easings, and
// continuous oscillators (pulses, waves, shimmers).
#include <cmath>
#include <algorithm>
#include "ui/core/math.hpp"
#include "ui/core/easing.hpp"

namespace ui {

// --- Advanced Easing Curves ---

inline float EaseInQuad(float t) { t = Clamp01(t); return t * t; }
inline float EaseOutQuad(float t) { t = Clamp01(t); return t * (2.f - t); }

inline float EaseOutBounce(float t) {
    t = Clamp01(t);
    constexpr float n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.f / d1) {
        return n1 * t * t;
    } else if (t < 2.f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

inline float EaseInBounce(float t) { return 1.f - EaseOutBounce(1.f - t); }

inline float EaseOutElastic(float t) {
    t = Clamp01(t);
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    constexpr float c4 = (2.f * 3.14159265f) / 3.f;
    return std::pow(2.f, -10.f * t) * std::sin((t * 10.f - 0.75f) * c4) + 1.f;
}

// --- Spring Physics Engine ---

struct SpringConfig {
    float stiffness = 180.f;  // tension (higher = snappier)
    float damping   = 16.f;   // friction (higher = less oscillation)
    float mass      = 1.f;

    static SpringConfig Bouncy()  { return {220.f, 12.f, 1.f}; } // playful bounce
    static SpringConfig Smooth()  { return {160.f, 20.f, 1.f}; } // critically damped
    static SpringConfig Snappy()  { return {320.f, 28.f, 1.f}; } // fast UI snap
};

struct Spring {
    float value = 0.f;
    float velocity = 0.f;
    float target = 0.f;
    SpringConfig config = SpringConfig::Smooth();

    Spring() = default;
    Spring(float initial, const SpringConfig& cfg = SpringConfig::Smooth())
        : value(initial), target(initial), config(cfg) {}

    void SetTarget(float newTarget) { target = newTarget; }
    void Reset(float val) { value = target = val; velocity = 0.f; }

    // Semi-implicit Euler integration (stable and fast for UI frame rates).
    float Update(float dt) {
        if (dt <= 0.f) return value;
        const int steps = std::clamp(static_cast<int>(std::ceil(dt * 120.f)), 1, 8);
        const float subDt = dt / static_cast<float>(steps);
        for (int i = 0; i < steps; ++i) {
            const float force = -config.stiffness * (value - target) - config.damping * velocity;
            const float accel = force / std::max(config.mass, 0.01f);
            velocity += accel * subDt;
            value += velocity * subDt;
        }
        return value;
    }

    bool Settled(float eps = 0.001f) const {
        return std::abs(value - target) < eps && std::abs(velocity) < eps;
    }
};

// --- Continuous Oscillators & Visual Effects ---

// Periodic breathing pulse in range [minVal, maxVal].
inline float Pulse(float timeSec, float freqHz, float minVal = 0.f, float maxVal = 1.f) {
    const float s = 0.5f + 0.5f * std::sin(timeSec * (2.f * 3.14159265f * freqHz));
    return Lerp(minVal, maxVal, s);
}

// Color pulse between two colors.
inline Color PulseColor(float timeSec, float freqHz, const Color& c1, const Color& c2) {
    const float t = Pulse(timeSec, freqHz, 0.f, 1.f);
    return Color::Blend(c1, c2, t);
}

// Progress wave / shimmer phase [0..1] over period.
inline float ShimmerPhase(float timeSec, float periodSec) {
    if (periodSec <= 0.f) return 0.f;
    return std::fmod(timeSec, periodSec) / periodSec;
}

} // namespace ui
