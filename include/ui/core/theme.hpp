#pragma once
// aris-ui — design tokens (Tailwind / Material-style theme)
#include "ui/core/math.hpp"

namespace ui {

struct Theme {
    // --- palette ---
    Color panelBg       = Color::Hex(0x15191F);   // panel surface
    Color panelBorder   = Color::Hex(0x2A3038);
    Color panelTitle    = Color::Hex(0xE6EDF3);

    Color text          = Color::Hex(0xE6EDF3);
    Color textDim       = Color::Hex(0x8B949E);
    Color textOnAccent  = Color::Hex(0xFFFFFF);

    Color accent        = Color::Hex(0x7C5CFF);   // violet accent
    Color accentHover   = Color::Hex(0x9277FF);
    Color accentActive  = Color::Hex(0x6A4CE0);

    Color controlBg     = Color::Hex(0x232830);   // inactive control surface
    Color controlBgHover= Color::Hex(0x2C323C);
    Color controlBorder = Color::Hex(0x30363D);
    Color trackBg       = Color::Hex(0x30363D);   // toggle track / slider track

    // --- metrics ---
    float panelRadius   = 12.f;
    float controlRadius = 8.f;
    float smallRadius   = 6.f;
    float borderWidth   = 1.f;

    Vec2  shadowOffset  = Vec2(0.f, 6.f);
    float shadowBlur    = 16.f;
    float shadowOpacity = 0.28f;

    float fontSize      = 15.f;   // default text size in UI px
    float rowGap        = 10.f;   // vertical gap between widgets
    float columnGap     = 12.f;   // horizontal gap between columns
    float controlHeight = 34.f;   // button height
    float smallHeight   = 26.f;   // toggle row height

    static const Theme& Default();
};

} // namespace ui