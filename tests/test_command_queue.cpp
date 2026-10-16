// aris-ui tests — POD command queue (no GL involved).
#include "test_main.hpp"
#include "ui/renderer/command_queue.hpp"

using namespace ui;

TEST(queue_starts_empty) {
    CommandQueue q;
    CHECK(q.Empty());
    CHECK_EQ(q.Commands().size(), 0u);
    CHECK_EQ(q.ClipDepth(), 0);
}

TEST(push_rect_records_command) {
    CommandQueue q;
    RectStyle st;
    st.fill = Color::Hex(0x7C5CFF);
    st.cornerRadii = Vec4(8.f);
    q.PushRect(Rect(10.f, 10.f, 100.f, 40.f), st);

    CHECK_EQ(q.Commands().size(), 1u);
    const DrawCommand& c = q.Commands()[0];
    CHECK(c.type == DrawCommand::Type::RoundedRect);
    CHECK_EQ(c.pos, (Vec2(10.f, 10.f)));
    CHECK_EQ(c.size, (Vec2(100.f, 40.f)));
    CHECK_EQ(c.color[0], 0x7C / 255.f);
    CHECK_EQ(c.radii, (Vec4(8.f)));
    CHECK_EQ(c.borderWidth, 0.f);
}

TEST(push_rect_copies_full_style) {
    CommandQueue q;
    RectStyle st;
    st.fill = Color::White();
    st.borderColor = Color::Hex(0x30363D);
    st.borderWidth = 1.5f;
    st.shadowOffset = Vec2(0.f, 6.f);
    st.shadowBlur = 16.f;
    st.shadowOpacity = 0.28f;
    st.shadowSpread = 2.f;
    q.PushRect(Rect(0.f, 0.f, 1.f, 1.f), st);

    const DrawCommand& c = q.Commands()[0];
    CHECK_EQ(c.borderColor[0], 0x30 / 255.f);
    CHECK_EQ(c.borderWidth, 1.5f);
    CHECK_EQ(c.shadowOffset, (Vec2(0.f, 6.f)));
    CHECK_EQ(c.shadowBlur, 16.f);
    CHECK_EQ(c.shadowOpacity, 0.28f);
    CHECK_EQ(c.shadowSpread, 2.f);
}

TEST(push_glyph_records_uv_and_atlas) {
    CommandQueue q;
    const float uv[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    q.PushGlyph(Rect(5.f, 6.f, 7.f, 8.f), uv, Color::White(), 42u, 15.f);

    CHECK_EQ(q.Commands().size(), 1u);
    const DrawCommand& c = q.Commands()[0];
    CHECK(c.type == DrawCommand::Type::Glyph);
    CHECK_EQ(c.pos, (Vec2(5.f, 6.f)));
    CHECK_EQ(c.size, (Vec2(7.f, 8.f)));
    CHECK_EQ(c.u0, 0.1f);
    CHECK_EQ(c.v1, 0.4f);
    CHECK_EQ(c.atlasId, 42u);
    CHECK_EQ(c.fontSize, 15.f);
}

TEST(push_triangle_records_points) {
    CommandQueue q;
    q.PushTriangle(Vec2(1.f, 2.f), Vec2(8.f, 2.f), Vec2(4.f, 9.f), Color::White(), 1.25f);
    CHECK_EQ(q.Commands().size(), 1u);
    const DrawCommand& c = q.Commands()[0];
    CHECK(c.type == DrawCommand::Type::Triangle);
    CHECK_EQ(c.pos, (Vec2(1.f, 2.f)));
    CHECK_EQ(c.size, (Vec2(8.f, 2.f)));
    CHECK_EQ(c.shadowOffset, (Vec2(4.f, 9.f)));
    CHECK_EQ(c.borderWidth, 1.25f);
}

TEST(clip_push_pop_tracks_depth) {
    CommandQueue q;
    q.PushClip(Rect(0.f, 0.f, 50.f, 50.f));
    q.PushClip(Rect(10.f, 10.f, 20.f, 20.f));
    CHECK_EQ(q.ClipDepth(), 2);
    CHECK_EQ(q.Commands().size(), 2u);
    CHECK(q.Commands()[0].type == DrawCommand::Type::ClipPush);

    q.PopClip();
    CHECK_EQ(q.ClipDepth(), 1);
    CHECK(q.Commands()[2].type == DrawCommand::Type::ClipPop);

    q.Reset();
    CHECK(q.Empty());
    CHECK_EQ(q.ClipDepth(), 0);
}