#pragma once
// aris-ui — POD draw command queue.
// The core engine records typed commands each frame; the OpenGL backend
// replays them into GPU batches. Pure data — no GL types here.
#include <cstdint>
#include <vector>
#include "ui/core/math.hpp"

namespace ui {

// Per-corner radius packing: (topLeft, topRight, bottomRight, bottomLeft)
// in a y-down coordinate system.
struct RectStyle {
    Color fill = Color::White();
    Color borderColor = Color::Transparent();
    float borderWidth = 0.f;
    Vec4 cornerRadii = Vec4(0.f);      // per-corner radii
    Vec2 shadowOffset = Vec2(0.f, 0.f);
    float shadowSpread = 0.f;          // shadow shape growth (px)
    float shadowBlur = 0.f;            // softness falloff (px)
    float shadowOpacity = 0.f;
    Color shadowColor = Color::Black(); // tint the shadow to make a colored glow

    // Linear gradient: fill -> fillEnd along gradientDir (y-down; (0,0) =
    // solid fill). e.g. (0,1) top->bottom, (1,0) left->right, (1,1) diagonal.
    Color fillEnd = Color::Transparent();
    Vec2 gradientDir = Vec2(0.f, 0.f);
};

struct DrawCommand {
    enum class Type : std::uint8_t {
        RoundedRect,
        Glyph,
        Line,       // round-capped AA segment: pos -> size (end point); borderWidth = thickness
        Backdrop,   // blurs what is already drawn beneath rect (frosted glass); borderWidth = blur radius px, color = tint
        ClipPush,
        ClipPop,
    };

    Type type = Type::RoundedRect;

    // rect geometry (RoundedRect) / glyph quad position+size (Glyph)
    Vec2 pos;
    Vec2 size;

    Vec4 radii;             // RoundedRect: per-corner radii
    Vec4 color;             // RoundedRect: fill color (0..1) / Glyph: text color
    Vec4 borderColor;       // RoundedRect: border color
    float borderWidth = 0.f; // Glyph: embolden weight in px (negative = thinner)

    Vec2 shadowOffset;
    float shadowSpread = 0.f;
    float shadowBlur = 0.f;
    float shadowOpacity = 0.f;
    Vec4 shadowColor = Vec4(0.f, 0.f, 0.f, 1.f);
    Vec4 colorEnd;             // gradient end color (0..1)
    Vec2 gradientDir;          // (0,0) = solid

    // Glyph data
    float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f;
    std::uint32_t atlasId = 0;   // font atlas handle (MVP: single atlas)
    float fontSize = 0.f;        // target glyph size in px (drives px-range scaling)
};

class CommandQueue {
public:
    void Reset();
    void PushRect(const Rect& rect, const RectStyle& style);
    void PushGlyph(const Rect& quad, const float uv[4], const Color& color,
                   std::uint32_t atlasId, float fontSize, float weight = 0.f);
    void PushLine(Vec2 a, Vec2 b, float thickness, const Color& color);
    void PushBackdrop(const Rect& rect, const Vec4& radii, float blurRadius, const Color& tint);
    void PushClip(const Rect& rect);
    void PopClip();

    const std::vector<DrawCommand>& Commands() const { return commands_; }
    std::vector<DrawCommand>& Commands() { return commands_; }

    int ClipDepth() const { return clipDepth_; }
    bool Empty() const { return commands_.empty(); }

private:
    std::vector<DrawCommand> commands_;
    int clipDepth_ = 0;
};

} // namespace ui