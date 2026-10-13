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
};

struct DrawCommand {
    enum class Type : std::uint8_t {
        RoundedRect,
        Glyph,
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
    float borderWidth = 0.f;

    Vec2 shadowOffset;
    float shadowSpread = 0.f;
    float shadowBlur = 0.f;
    float shadowOpacity = 0.f;

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
                   std::uint32_t atlasId, float fontSize);
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