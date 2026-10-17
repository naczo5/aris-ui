#include "ui/renderer/command_queue.hpp"

namespace ui {

void CommandQueue::Reset() {
    commands_.clear();
    clipDepth_ = 0;
}

void CommandQueue::PushRect(const Rect& rect, const RectStyle& style) {
    DrawCommand& c = commands_.emplace_back();
    c.type = DrawCommand::Type::RoundedRect;
    c.pos = rect.Pos();
    c.size = rect.Size();
    c.radii = style.cornerRadii;
    c.color = style.fill.ToVec4();
    c.borderColor = style.borderColor.ToVec4();
    c.borderWidth = style.borderWidth;
    c.shadowOffset = style.shadowOffset;
    c.shadowSpread = style.shadowSpread;
    c.shadowBlur = style.shadowBlur;
    c.shadowOpacity = style.shadowOpacity;
    c.shadowColor = style.shadowColor.ToVec4();
    c.colorEnd = style.fillEnd.ToVec4();
    c.gradientDir = style.gradientDir;
}

void CommandQueue::PushTriangle(Vec2 a, Vec2 b, Vec2 c, const Color& color, float stroke) {
    DrawCommand& cmd = commands_.emplace_back();
    cmd.type = DrawCommand::Type::Triangle;
    cmd.pos = a;
    cmd.size = b;
    cmd.shadowOffset = c;
    cmd.color = color.ToVec4();
    cmd.borderWidth = stroke;
}

void CommandQueue::PushLine(Vec2 a, Vec2 b, float thickness, const Color& color) {
    DrawCommand& c = commands_.emplace_back();
    c.type = DrawCommand::Type::Line;
    c.pos = a;
    c.size = b;
    c.color = color.ToVec4();
    c.borderWidth = thickness;
}

void CommandQueue::PushBackdrop(const Rect& rect, const Vec4& radii, float blurRadius,
                                const Color& tint) {
    DrawCommand& c = commands_.emplace_back();
    c.type = DrawCommand::Type::Backdrop;
    c.pos = rect.Pos();
    c.size = rect.Size();
    c.radii = radii;
    c.color = tint.ToVec4();
    c.borderWidth = blurRadius;
}

void CommandQueue::PushGlyph(const Rect& quad, const float uv[4], const Color& color,
                             std::uint32_t atlasId, float fontSize, float weight) {
    DrawCommand& c = commands_.emplace_back();
    c.type = DrawCommand::Type::Glyph;
    c.pos = quad.Pos();
    c.size = quad.Size();
    c.color = color.ToVec4();
    c.u0 = uv[0]; c.v0 = uv[1]; c.u1 = uv[2]; c.v1 = uv[3];
    c.atlasId = atlasId;
    c.fontSize = fontSize;
    c.borderWidth = weight;   // glyphs reuse borderWidth as embolden (px)
}

void CommandQueue::PushClip(const Rect& rect) {
    DrawCommand& c = commands_.emplace_back();
    c.type = DrawCommand::Type::ClipPush;
    c.pos = rect.Pos();
    c.size = rect.Size();
    ++clipDepth_;
}

void CommandQueue::PopClip() {
    if (clipDepth_ <= 0) return;
    DrawCommand& c = commands_.emplace_back();
    c.type = DrawCommand::Type::ClipPop;
    --clipDepth_;
}

} // namespace ui