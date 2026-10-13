#pragma once
// aris-ui — "kit": higher-level, absolutely-positioned components for
// overlay / ClickGUI-style interfaces (glass panels, nav pills, module rows,
// switches). Everything is built on the same public primitives you have
// (DrawRect with gradients/glow, DrawLine, DrawText weight, Interact), so
// custom components look and animate exactly like the built-ins.
#include "ui/core/context.hpp"
#include "ui/widgets/icons.hpp"

namespace ui::kit {

// Design tokens for the kit. `Aurora()` is a deep-violet glass look.
struct Palette {
    Color panelTop, panelBottom;      // translucent surface gradient
    Color border, borderHighlight;    // hairline + top-edge sheen
    Color divider;
    Color text, textDim, textFaint;
    Color accent, accentEnd, accentGlow;
    Color hover;                      // additive-ish hover wash
    Color track;                      // switch / slider track (off)
    float blurRadius;                 // backdrop blur behind glass surfaces (px, 0 = off)
};

const Palette& Aurora();

// Frosted-glass window surface: gradient fill, sheen border, soft glow + drop shadow.
void GlassPanel(Context& ctx, const Rect& rect, const Palette& pal, float radius = 20.f);

// Sidebar entry: icon + label with an animated accent pill when selected.
// Returns true when clicked.
bool NavItem(Context& ctx, const Palette& pal, const char* id, const Rect& rect,
             Icon icon, const char* label, bool selected);

// Feature row: title + description (+ optional animated mini switch).
// Clicking toggles *enabled. Returns true when toggled this frame.
bool ModuleRow(Context& ctx, const Palette& pal, const char* id, const Rect& rect,
               const char* name, const char* description, bool* enabled, bool withSwitch = false);

// Circular icon button (e.g. search). Returns true when clicked.
bool IconButton(Context& ctx, const Palette& pal, const char* id, const Rect& rect, Icon icon);

// Small caps-style section heading.
void SectionTitle(Context& ctx, const Palette& pal, Vec2 pos, const char* text);

// Round avatar with initial.
void Avatar(Context& ctx, const Palette& pal, Vec2 center, float radius, const char* initial);

// Pure-draw switch (no input) — reusable inside custom rows. t in [0,1].
void SwitchGraphic(Context& ctx, const Palette& pal, const Rect& rect, float t);

} // namespace ui::kit
