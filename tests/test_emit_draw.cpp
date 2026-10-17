// Regression tests for the CPU side of the batchers: BuildUiSpans turns the
// draw-command queue into scissor-grouped vertex spans. No GL, no Vulkan.
#include "test_main.hpp"

#include "emit_draw.hpp"
#include "ui/renderer/command_queue.hpp"

using namespace ui;

namespace {

// A rect tall enough to touch everything a span can carry.
void PushRect(CommandQueue& q) {
    q.PushRect(Rect(10.f, 20.f, 30.f, 40.f), RectStyle{});
}

std::size_t TotalVerts(const SpanScratch& scratch) {
    std::size_t n = 0;
    for (const UiSpan& s : scratch.spans) n += s.verts.size();
    return n;
}

} // namespace

TEST(spans_merge_shape_commands_into_one_span) {
    CommandQueue q;
    PushRect(q);
    PushRect(q);
    q.PushLine(Vec2(0.f, 0.f), Vec2(10.f, 10.f), 2.f, Color::White());
    PushRect(q);

    SpanScratch scratch;
    BuildUiSpans(q, scratch);

    CHECK_EQ(scratch.spans.size(), std::size_t(1));
    CHECK_EQ(scratch.spans[0].atlasId, 0u);
    CHECK_EQ(scratch.spans[0].verts.size(), std::size_t(4 * 6));   // one quad per command
    CHECK_FALSE(scratch.spans[0].scissor);
}

TEST(spans_split_on_glyph_batch_key) {
    CommandQueue q;
    const float uv[4] = {0.f, 0.f, 0.5f, 0.5f};
    q.PushGlyph(Rect(0.f, 0.f, 8.f, 8.f), uv, Color::White(), 1u, 12.f);
    q.PushGlyph(Rect(8.f, 0.f, 8.f, 8.f), uv, Color::White(), 1u, 12.f);
    q.PushGlyph(Rect(16.f, 0.f, 8.f, 8.f), uv, Color::White(), 1u, 18.f);   // new font size
    q.PushGlyph(Rect(24.f, 0.f, 8.f, 8.f), uv, Color::White(), 2u, 18.f);   // new atlas

    SpanScratch scratch;
    BuildUiSpans(q, scratch);

    CHECK_EQ(scratch.spans.size(), std::size_t(3));
    CHECK_EQ(scratch.spans[0].verts.size(), std::size_t(12));
    CHECK_EQ(scratch.spans[0].atlasId, 1u);
    CHECK_EQ(scratch.spans[0].fontSize, 12.f);
    CHECK_EQ(scratch.spans[1].verts.size(), std::size_t(6));
    CHECK_EQ(scratch.spans[2].atlasId, 2u);
    CHECK_EQ(TotalVerts(scratch), std::size_t(24));
}

TEST(spans_carry_scissor_from_clip_stack) {
    CommandQueue q;
    PushRect(q);                                   // unclipped
    q.PushClip(Rect(100.f, 100.f, 50.f, 50.f));
    PushRect(q);                                   // clipped to the inner rect
    q.PushClip(Rect(110.f, 110.f, 20.f, 20.f));     // nesting intersects
    PushRect(q);
    q.PopClip();
    PushRect(q);
    q.PopClip();
    PushRect(q);

    SpanScratch scratch;
    BuildUiSpans(q, scratch);

    CHECK_EQ(scratch.spans.size(), std::size_t(5));
    CHECK_FALSE(scratch.spans[0].scissor);
    CHECK(scratch.spans[1].scissor);
    CHECK_EQ(scratch.spans[1].scissorX, 100.f);
    CHECK_EQ(scratch.spans[1].scissorY, 100.f);
    CHECK_EQ(scratch.spans[1].scissorW, 50.f);
    CHECK(scratch.spans[2].scissor);
    CHECK_EQ(scratch.spans[2].scissorX, 110.f);
    CHECK_EQ(scratch.spans[2].scissorW, 20.f);
    CHECK(scratch.spans[3].scissor);                // back to the outer clip
    CHECK_EQ(scratch.spans[3].scissorW, 50.f);
    CHECK_FALSE(scratch.spans[4].scissor);
}

TEST(spans_fully_clipped_out_become_empty_scissors) {
    CommandQueue q;
    q.PushClip(Rect(0.f, 0.f, 10.f, 10.f));
    q.PushClip(Rect(100.f, 100.f, 10.f, 10.f));     // no overlap
    PushRect(q);
    q.PopClip();

    SpanScratch scratch;
    BuildUiSpans(q, scratch);

    CHECK_EQ(scratch.spans.size(), std::size_t(1));
    // A zero-size scissor, not "no scissor": the draw must be dropped, not
    // promoted to an unclipped one.
    CHECK(scratch.spans[0].scissor);
    CHECK(scratch.spans[0].scissorW <= 0.f);
    CHECK(scratch.spans[0].scissorH <= 0.f);
}

TEST(spans_clip_overflow_keeps_pops_balanced) {
    CommandQueue q;
    for (int i = 0; i < 31; ++i)
        q.PushClip(Rect(0.f, 0.f, 200.f, 200.f));   // C1..C31
    q.PushClip(Rect(0.f, 0.f, 5.f, 5.f));           // C32: the last live clip
    q.PushClip(Rect(0.f, 0.f, 200.f, 200.f));       // C33: past the fixed stack, dropped
    q.PopClip();                                    // must pop the dropped push, not C32
    PushRect(q);

    SpanScratch scratch;
    BuildUiSpans(q, scratch);

    CHECK_EQ(scratch.spans.size(), std::size_t(1));
    CHECK(scratch.spans[0].scissor);
    CHECK_EQ(scratch.spans[0].scissorW, 5.f);       // losing C32 would widen this to 200
    CHECK_EQ(scratch.spans[0].scissorH, 5.f);
}

TEST(spans_pack_triangle_bbox_and_points) {
    CommandQueue q;
    q.PushTriangle(Vec2(10.f, 20.f), Vec2(50.f, 20.f), Vec2(30.f, 60.f), Color::White(), 0.f);

    SpanScratch scratch;
    BuildUiSpans(q, scratch);

    CHECK_EQ(scratch.spans.size(), std::size_t(1));
    const std::vector<UiVertex>& v = scratch.spans[0].verts;
    CHECK_EQ(v.size(), std::size_t(6));
    // Content rect is the triangle's bounding box, grown by `pad` for the AA edge.
    CHECK_EQ(v[0].rect[0], 10.f);
    CHECK_EQ(v[0].rect[1], 20.f);
    CHECK_EQ(v[0].rect[2], 40.f);
    CHECK_EQ(v[0].rect[3], 40.f);
    CHECK_EQ(v[0].params[3], 3.f);                  // kind: triangle
    CHECK_EQ(v[0].shadow[3], 2.f);                  // pad
    CHECK_EQ(v[0].pos[0], 0.f);
    CHECK_EQ(v[0].pos[1], 0.f);
    // Points are stored relative to the bbox origin.
    CHECK_EQ(v[0].radii[0], 0.f);
    CHECK_EQ(v[0].radii[1], 0.f);
    CHECK_EQ(v[0].radii[2], 40.f);
    CHECK_EQ(v[0].radii[3], 0.f);
    CHECK_EQ(v[0].extra[0], 20.f);
    CHECK_EQ(v[0].extra[1], 40.f);
    CHECK_EQ(v[0].color[3], 255);                   // opaque white, premultiplied later
}

TEST(spans_pack_line_kind_and_stroke_pad) {
    CommandQueue q;
    q.PushLine(Vec2(100.f, 50.f), Vec2(40.f, 50.f), 4.f, Color::White());

    SpanScratch scratch;
    BuildUiSpans(q, scratch);

    CHECK_EQ(scratch.spans.size(), std::size_t(1));
    const UiVertex& v = scratch.spans[0].verts[0];
    CHECK_EQ(v.params[3], 2.f);                     // kind: line
    CHECK_EQ(v.params[0], 4.f);                     // thickness
    CHECK_EQ(v.rect[0], 40.f);                      // bbox handles reversed endpoints
    CHECK_EQ(v.rect[2], 60.f);
    CHECK_EQ(v.shadow[3], 4.f);                     // pad = thickness/2 + 2
    CHECK_EQ(v.radii[0], 60.f);                     // first endpoint, local
    CHECK_EQ(v.radii[2], 0.f);
}

TEST(spans_reuse_scratch_across_frames) {
    CommandQueue q1;
    PushRect(q1);
    q1.PushClip(Rect(0.f, 0.f, 5.f, 5.f));
    PushRect(q1);

    SpanScratch scratch;
    BuildUiSpans(q1, scratch);
    CHECK_EQ(scratch.spans.size(), std::size_t(2));

    // A shorter frame must shrink the span list, a longer one grow it again.
    CommandQueue q2;
    PushRect(q2);
    BuildUiSpans(q2, scratch);
    CHECK_EQ(scratch.spans.size(), std::size_t(1));
    CHECK_EQ(TotalVerts(scratch), std::size_t(6));

    CommandQueue q3;
    PushRect(q3);
    q3.PushClip(Rect(0.f, 0.f, 5.f, 5.f));
    PushRect(q3);
    q3.PushClip(Rect(1.f, 1.f, 2.f, 2.f));
    PushRect(q3);
    BuildUiSpans(q3, scratch);
    CHECK_EQ(scratch.spans.size(), std::size_t(3));
    CHECK_EQ(TotalVerts(scratch), std::size_t(18));
}

TEST(spans_backdrop_is_its_own_empty_span) {
    CommandQueue q;
    PushRect(q);
    q.PushBackdrop(Rect(0.f, 0.f, 40.f, 40.f), Vec4(8.f), 6.f, Color(0, 0, 0, 120));
    PushRect(q);

    SpanScratch scratch;
    BuildUiSpans(q, scratch);

    CHECK_EQ(scratch.spans.size(), std::size_t(3));
    CHECK_FALSE(scratch.spans[0].backdrop);
    CHECK(scratch.spans[1].backdrop);
    CHECK(scratch.spans[1].verts.empty());          // GL-only effect, no geometry
    CHECK((int)scratch.spans[1].backdropCmd.type == (int)DrawCommand::Type::Backdrop);
    CHECK_FALSE(scratch.spans[2].backdrop);
}
