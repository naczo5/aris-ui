#pragma once
// aris-ui — built-in vector icons.
//
// Icons are drawn from anti-aliased round-capped line segments and circle
// SDFs (see Context::DrawLine / DrawCircle), so they stay razor sharp at any
// size and take any color — no icon font or texture required.
#include "ui/core/context.hpp"

namespace ui {

enum class Icon {
    Combat,     // crossed swords
    Movement,   // runner
    Player,     // person
    Visuals,    // eye
    Settings,   // gear
    Friends,    // two people
    Configs,    // folder
    Hud,        // monitor
    Search,     // magnifier
    Check,
    ChevronRight,
    ChevronDown,
    Close,
    Plus,
    Bolt,
    Shield,
    GrimCrest,  // shield crest emblem (Grim client style)
    Cursor,     // mouse pointer arrow
    Count
};

// Draws `icon` centered in the square fitted into `box`. `stroke` is the
// line thickness in pixels *at 24px icon size*; it scales with the box.
void DrawIcon(Context& ctx, Icon icon, const Rect& box, const Color& color,
              float stroke = 1.7f);

} // namespace ui
