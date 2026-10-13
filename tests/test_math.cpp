// aris-ui tests — math primitives (Vec2/Vec4/Rect/Color).
#include "test_main.hpp"
#include "ui/core/math.hpp"

using namespace ui;

TEST(vec2_arithmetic) {
    Vec2 a(1.f, 2.f), b(3.f, -1.f);
    CHECK_EQ(a + b, (Vec2(4.f, 1.f)));
    CHECK_EQ(a - b, (Vec2(-2.f, 3.f)));
    CHECK_EQ(b * 2.f, (Vec2(6.f, -2.f)));
    a += b;
    CHECK_EQ(a, (Vec2(4.f, 1.f)));
    CHECK_NEAR(a.Length(), 4.1231, 1e-3);
    CHECK_NEAR(a.Dot(b), 11.f, 1e-5);
}

TEST(rect_geometry) {
    Rect r(10.f, 20.f, 30.f, 40.f);
    CHECK(r.Valid());
    CHECK(r.Contains(Vec2(10.f, 20.f)));
    CHECK(r.Contains(Vec2(40.f, 60.f)));
    CHECK_FALSE(r.Contains(Vec2(41.f, 20.f)));
    CHECK_EQ(r.Center(), (Vec2(25.f, 40.f)));
    CHECK_EQ(r.Max(), (Vec2(40.f, 60.f)));
    CHECK_EQ(r.Inset(5.f), (Rect(15.f, 25.f, 20.f, 30.f)));
    CHECK_EQ(r.Offset(Vec2(1.f, 2.f)), (Rect(11.f, 22.f, 30.f, 40.f)));
    Rect empty(0.f, 0.f, -1.f, 10.f);
    CHECK_FALSE(empty.Valid());
}

TEST(color_hex_and_blend) {
    Color c = Color::Hex(0x7C5CFF);
    CHECK_EQ(c.r, 0x7C);
    CHECK_EQ(c.g, 0x5C);
    CHECK_EQ(c.b, 0xFF);
    CHECK_EQ(c.a, 255);

    Color ca = Color::Hex(0xFF0000, 128);
    CHECK_EQ(ca.a, 128);

    CHECK_EQ(Color::White(), (Color(255, 255, 255, 255)));
    CHECK_EQ(Color::Transparent(), (Color(0, 0, 0, 0)));

    Color mid = Color::Blend(Color::Black(), Color::White(), 0.5f);
    CHECK_EQ(mid.r, 128);   // 0 + 255*0.5 + 0.5 rounds to 128
    CHECK_EQ(mid.b, 128);

    Vec4 f = Color::Hex(0xFF8000).ToVec4();
    CHECK_NEAR(f.x, 1.f, 1e-6);
    CHECK_NEAR(f.y, 128.f / 255.f, 1e-6);
}

TEST(color_with_alpha) {
    Color c = Color::Hex(0x112233, 255).WithAlpha(64);
    CHECK_EQ(c.r, 0x11);
    CHECK_EQ(c.a, 64);
}