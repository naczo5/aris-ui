#include "emit_draw.hpp"

#include <algorithm>
#include <cmath>

namespace ui {
namespace {

void PackColor(std::uint8_t out[4], const Vec4& c) {
    out[0] = (std::uint8_t)(c.x * 255.f + 0.5f);
    out[1] = (std::uint8_t)(c.y * 255.f + 0.5f);
    out[2] = (std::uint8_t)(c.z * 255.f + 0.5f);
    out[3] = (std::uint8_t)(c.w * 255.f + 0.5f);
}

void PushQuad(std::vector<UiVertex>& dst, const UiVertex& proto) {
    static const float kQuad[6][2] = {
        {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f},
        {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f},
    };
    for (const auto& corner : kQuad) {
        UiVertex v = proto;
        v.pos[0] = corner[0];
        v.pos[1] = corner[1];
        dst.push_back(v);
    }
}

bool IntersectClip(const Rect* stack, int depth, float& x, float& y, float& w, float& h) {
    if (depth <= 0) return false;
    float x0 = stack[0].x, y0 = stack[0].y;
    float x1 = x0 + stack[0].w, y1 = y0 + stack[0].h;
    for (int i = 1; i < depth; ++i) {
        x0 = std::max(x0, stack[i].x);
        y0 = std::max(y0, stack[i].y);
        x1 = std::min(x1, stack[i].x + stack[i].w);
        y1 = std::min(y1, stack[i].y + stack[i].h);
    }
    w = x1 - x0;
    h = y1 - y0;
    x = x0;
    y = y0;
    return w > 0.f && h > 0.f;
}

} // namespace

void BuildUiSpans(const CommandQueue& queue, SpanScratch& scratch) {
    std::vector<UiSpan>& spans = scratch.spans;
    std::size_t spanCount = 0;
    UiSpan* cur = nullptr;
    Rect clipStack[32];
    int clipDepth = 0;

    // Open (or reuse) the next span slot and stamp it with the current clip
    // stack. Existing vector capacity is kept across frames.
    auto openSpan = [&]() -> UiSpan& {
        if (spanCount == spans.size()) spans.emplace_back();
        UiSpan& span = spans[spanCount++];
        span.backdrop = false;
        span.atlasId = 0;
        span.fontSize = -1.f;
        span.scissorX = span.scissorY = span.scissorW = span.scissorH = 0.f;
        span.verts.clear();
        span.scissor = IntersectClip(clipStack, clipDepth, span.scissorX, span.scissorY,
                                     span.scissorW, span.scissorH);
        if (clipDepth > 0 && !span.scissor) {
            span.scissor = true;
            span.scissorW = 0.f;
            span.scissorH = 0.f;
        }
        return span;
    };
    // Current accumulation target; opened lazily so empty spans are not emitted.
    auto current = [&]() -> UiSpan& {
        if (!cur) cur = &openSpan();
        return *cur;
    };
    auto close = [&]() { cur = nullptr; };

    for (const DrawCommand& cmd : queue.Commands()) {
        switch (cmd.type) {
        case DrawCommand::Type::ClipPush:
            close();
            if (clipDepth < 32)
                clipStack[clipDepth++] = Rect(cmd.pos.x, cmd.pos.y, cmd.size.x, cmd.size.y);
            break;
        case DrawCommand::Type::ClipPop:
            close();
            if (clipDepth > 0) --clipDepth;
            break;
        case DrawCommand::Type::Backdrop: {
            close();
            UiSpan& span = openSpan();
            span.backdrop = true;
            span.backdropCmd = cmd;
            break;
        }
        case DrawCommand::Type::Glyph:
            if (!cur || cur->atlasId != cmd.atlasId || cur->fontSize != cmd.fontSize) {
                close();
                UiSpan& span = openSpan();
                span.atlasId = cmd.atlasId;
                span.fontSize = cmd.fontSize;
                cur = &span;
            }
            {
                UiVertex proto{};
                proto.rect[0] = cmd.pos.x; proto.rect[1] = cmd.pos.y;
                proto.rect[2] = cmd.size.x; proto.rect[3] = cmd.size.y;
                PackColor(proto.color, cmd.color);
                proto.params[0] = cmd.borderWidth;
                proto.params[3] = 1.f;
                proto.uv[0] = cmd.u0; proto.uv[1] = cmd.v0;
                proto.uv[2] = cmd.u1; proto.uv[3] = cmd.v1;
                PushQuad(cur->verts, proto);
            }
            break;
        case DrawCommand::Type::RoundedRect: {
            UiVertex proto{};
            const float pad = (cmd.shadowOpacity > 0.f)
                ? std::max(std::abs(cmd.shadowOffset.x), std::abs(cmd.shadowOffset.y)) +
                  cmd.shadowSpread + cmd.shadowBlur + 2.f
                : 0.f;
            proto.rect[0] = cmd.pos.x; proto.rect[1] = cmd.pos.y;
            proto.rect[2] = cmd.size.x; proto.rect[3] = cmd.size.y;
            proto.radii[0] = cmd.radii.x; proto.radii[1] = cmd.radii.y;
            proto.radii[2] = cmd.radii.z; proto.radii[3] = cmd.radii.w;
            PackColor(proto.color, cmd.color);
            PackColor(proto.borderColor, cmd.borderColor);
            proto.params[0] = cmd.borderWidth;
            proto.params[1] = cmd.shadowSpread;
            proto.params[2] = cmd.shadowBlur;
            proto.shadow[0] = cmd.shadowOffset.x;
            proto.shadow[1] = cmd.shadowOffset.y;
            proto.shadow[2] = cmd.shadowOpacity;
            proto.shadow[3] = pad;
            PackColor(proto.color2, cmd.colorEnd);
            PackColor(proto.shadowColor, cmd.shadowColor);
            proto.extra[0] = cmd.gradientDir.x;
            proto.extra[1] = cmd.gradientDir.y;
            PushQuad(current().verts, proto);
            break;
        }
        case DrawCommand::Type::Triangle: {
            UiVertex proto{};
            const Vec2 a = cmd.pos, b = cmd.size, c = cmd.shadowOffset;
            const float th = std::max(cmd.borderWidth, 0.f);
            const float pad = th * 0.5f + 2.f;
            const float minX = std::min(a.x, std::min(b.x, c.x));
            const float minY = std::min(a.y, std::min(b.y, c.y));
            const float maxX = std::max(a.x, std::max(b.x, c.x));
            const float maxY = std::max(a.y, std::max(b.y, c.y));
            proto.rect[0] = minX; proto.rect[1] = minY;
            proto.rect[2] = std::max(maxX - minX, 1.f);
            proto.rect[3] = std::max(maxY - minY, 1.f);
            proto.radii[0] = a.x - minX; proto.radii[1] = a.y - minY;
            proto.radii[2] = b.x - minX; proto.radii[3] = b.y - minY;
            proto.extra[0] = c.x - minX; proto.extra[1] = c.y - minY;
            PackColor(proto.color, cmd.color);
            proto.params[0] = th;
            proto.params[3] = 3.f;
            proto.shadow[3] = pad;
            PushQuad(current().verts, proto);
            break;
        }
        case DrawCommand::Type::Line: {
            UiVertex proto{};
            const Vec2 a = cmd.pos, b = cmd.size;
            const float th = std::max(cmd.borderWidth, 0.5f);
            const float pad = th * 0.5f + 2.f;
            const float bx = std::min(a.x, b.x), by = std::min(a.y, b.y);
            proto.rect[0] = bx; proto.rect[1] = by;
            proto.rect[2] = std::abs(a.x - b.x); proto.rect[3] = std::abs(a.y - b.y);
            proto.radii[0] = a.x - bx; proto.radii[1] = a.y - by;
            proto.radii[2] = b.x - bx; proto.radii[3] = b.y - by;
            PackColor(proto.color, cmd.color);
            proto.params[0] = th;
            proto.params[3] = 2.f;
            proto.shadow[3] = pad;
            PushQuad(current().verts, proto);
            break;
        }
        }
    }

    close();
    // Shrinking destroys the tail spans (rare); growing reuses slots.
    spans.resize(spanCount);
}

} // namespace ui
