#pragma once
// aris-ui — design tokens (Tailwind / Material-style theme)
#include "ui/core/math.hpp"

namespace ui {

struct Theme {
    // --- palette (Aurora: violet glass) ---
    Color panelBg       = Color::Hex(0x1A0F2E, 205);   // translucent surface (blur shows through)
    Color panelBorder   = Color(255, 255, 255, 28);
    Color panelTitle    = Color::Hex(0xF4F0FF);

    Color text          = Color::Hex(0xF4F0FF);
    Color textDim       = Color::Hex(0x9A90B8);
    Color textOnAccent  = Color::Hex(0xFFFFFF);

    Color accent        = Color::Hex(0xA45CFF);        // violet accent
    Color accentHover   = Color::Hex(0xB77DFF);
    Color accentActive  = Color::Hex(0x7B3FF2);        // also the gradient end

    Color controlBg     = Color(255, 255, 255, 14);    // inactive control surface
    Color controlBgHover= Color(255, 255, 255, 24);
    Color controlBorder = Color(255, 255, 255, 30);
    Color trackBg       = Color(255, 255, 255, 30);    // toggle track / slider track

    // --- metrics ---
    float panelRadius   = 18.f;
    float controlRadius = 10.f;
    float smallRadius   = 6.f;
    float borderWidth   = 1.f;
    float panelBlur     = 26.f;   // backdrop blur behind panels (px, 0 = off)

    Vec2  shadowOffset  = Vec2(0.f, 18.f);
    float shadowBlur    = 40.f;
    float shadowOpacity = 0.5f;


    float fontSize      = 15.f;   // default text size in UI px
    float rowGap        = 10.f;   // vertical gap between widgets
    float columnGap     = 12.f;   // horizontal gap between columns
    float controlHeight = 34.f;   // button height
    float smallHeight   = 26.f;   // toggle row height

    static const Theme& Default();
};

} // namespace ui