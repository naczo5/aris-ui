// aris-ui tests — easing / interpolation.
#include "test_main.hpp"
#include "ui/core/easing.hpp"

using namespace ui;

TEST(clamp01_and_lerp) {
    CHECK_EQ(Clamp01(-0.5f), 0.f);
    CHECK_EQ(Clamp01(0.25f), 0.25f);
    CHECK_EQ(Clamp01(2.f), 1.f);
    CHECK_NEAR(Lerp(0.f, 10.f, 0.3f), 3.f, 1e-6);
}

TEST(exp_smooth_converges_frame_rate_independently) {
    // 60 fps x 1s and 30 fps x 1s should land near the same value.
    float a = 0.f;
    for (int i = 0; i < 60; ++i) a = ExpSmooth(a, 1.f, 1.f / 60.f, 10.f);
    float b = 0.f;
    for (int i = 0; i < 30; ++i) b = ExpSmooth(b, 1.f, 2.f / 60.f, 10.f);
    CHECK_NEAR(a, b, 0.01f);
    CHECK(a > 0.99f);   // converged after ~1s at speed 10
    CHECK_EQ(ExpSmooth(0.5f, 1.f, 0.f, 10.f), 0.5f);   // dt=0 → unchanged
    CHECK_EQ(ExpSmooth(0.5f, 1.f, 0.016f, 0.f), 0.5f); // speed=0 → unchanged
}

TEST(exp_smooth_monotonic) {
    float v = 0.f;
    for (int i = 0; i < 30; ++i) {
        float nv = ExpSmooth(v, 1.f, 1.f / 60.f, 12.f);
        CHECK(nv >= v);
        v = nv;
    }
}

TEST(ease_out_cubic_endpoints) {
    CHECK_EQ(EaseOutCubic(0.f), 0.f);
    CHECK_EQ(EaseOutCubic(1.f), 1.f);
    CHECK_NEAR(EaseOutCubic(0.5f), 0.875f, 1e-5);
    CHECK(EaseOutCubic(0.7f) > 0.7f);   // out = fast start
}

TEST(ease_in_out_cubic_symmetric) {
    CHECK_NEAR(EaseInOutCubic(0.3f), 1.f - EaseInOutCubic(0.7f), 1e-5);
    CHECK_EQ(EaseInOutCubic(0.5f), 0.5f);
}

TEST(ease_out_back_overshoots) {
    float maxV = 0.f;
    for (int i = 0; i <= 100; ++i) maxV = std::max(maxV, EaseOutBack(i / 100.f));
    CHECK(maxV > 1.0f);   // classic back overshoot
    CHECK_EQ(EaseOutBack(0.f), 0.f);
    CHECK_EQ(EaseOutBack(1.f), 1.f);
}

TEST(cubic_bezier_matches_css_reference) {
    // cubic-bezier(0.25, 0.1, 0.25, 1.0) is the CSS "ease" curve.
    CHECK_NEAR(CubicBezierEase(0.f, Vec2(0.25f, 0.1f), Vec2(0.25f, 1.f)), 0.f, 1e-4);
    CHECK_NEAR(CubicBezierEase(1.f, Vec2(0.25f, 0.1f), Vec2(0.25f, 1.f)), 1.f, 1e-4);
    CHECK_NEAR(CubicBezierEase(0.5f, Vec2(0.5f, 0.5f), Vec2(0.5f, 0.5f)), 0.5f, 1e-4);
}