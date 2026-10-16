#pragma once
// Interleaved UI vertex. OpenGL and Vulkan bind the same layout.
#include <cstddef>
#include <cstdint>

namespace ui {

struct UiVertex {
    float pos[2];             // unit quad corner (0..1)
    float rect[4];            // content rect (screen px): x, y, w, h
    float radii[4];           // per-corner radii, or line/triangle endpoints
    std::uint8_t color[4];
    std::uint8_t borderColor[4];
    float params[4];          // borderWidth, shadowSpread, shadowBlur, kind
    float shadow[4];          // offsetX, offsetY, shadowOpacity, quadPad
    float uv[4];              // glyph uv rect (0 when unused)
    std::uint8_t color2[4];
    std::uint8_t shadowColor[4];
    float extra[4];           // gradientDir.xy, or the triangle's third point
};

static_assert(sizeof(UiVertex) == 120, "UI vertex layout drifted");
static_assert(offsetof(UiVertex, pos) == 0, "pos");
static_assert(offsetof(UiVertex, rect) == 8, "rect");
static_assert(offsetof(UiVertex, radii) == 24, "radii");
static_assert(offsetof(UiVertex, color) == 40, "color");
static_assert(offsetof(UiVertex, borderColor) == 44, "border");
static_assert(offsetof(UiVertex, params) == 48, "params");
static_assert(offsetof(UiVertex, shadow) == 64, "shadow");
static_assert(offsetof(UiVertex, uv) == 80, "uv");
static_assert(offsetof(UiVertex, color2) == 96, "color2");
static_assert(offsetof(UiVertex, shadowColor) == 100, "shadowColor");
static_assert(offsetof(UiVertex, extra) == 104, "extra");

} // namespace ui
