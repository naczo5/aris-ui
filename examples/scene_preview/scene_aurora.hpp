// aris-ui flagship preview: "Aurora" ClickGUI.
//
// A glass-morphism, game-client-style window built *entirely* from public
// library calls (ui::kit components, DrawRect gradients/glow, vector icons,
// text weight). This is the scene that shows what the library can do.
#pragma once
#include "common/AppSupport.hpp"
#include "ui/aris_ui.hpp"

#include <cmath>
#include <cstdio>

namespace aurora {

constexpr float kW = 1100.f, kH = 720.f;                 // scene size
constexpr ui::Rect kWin(60.f, 40.f, 980.f, 640.f);
constexpr float kSide = 240.f, kHead = 72.f;
constexpr float kRowH = 60.f, kRowGap = 6.f;

struct Module { const char* name; const char* desc; bool on; };

// Row-major: (left, right) pairs matching Grim Client layout.
inline Module* Modules() {
    static Module m[] = {
        {"Trigger Bot", "Automatically hits entity when in range", true},
        {"Auto Logout", "Logouts you from server when player is in distance", false},
        {"Auto Shield Break", "Automatically breaks shield", false},
        {"Hitbox", "Visible version of hitboxes", true},
        {"Kill Aura", "Automatically attacks entities", true},
        {"Target Margin", "Invisible version of hitboxes", false},
        {"Potion Refill", "Automatically refills potions", false},
        {"Aim Assist", "Automatically aims at entity", true},
        {"Backtrack", "Delays incoming packets to gain advantage", false},
        {"Auto Web", "Automatically places webs at enemies", false},
        {"Auto Trap", "Automatically traps enemy", false},
        {"Anti Bot", "Distincts real players from bots", false},
        {"Auto Potion", "Automatically uses potions", false},
        {"W-Tap", "Automatically performs W-Tap", true},
        {"Auto Drain", "Automatically pickups liquid from cobweb", false},
        {"Auto Dripstone", "Automatically places dripstone and opens trapdoor", false},
    };
    return m;
}

inline ui::Rect RowRect(int index) {
    const float x0 = kWin.x + kSide + 20.f, x1 = kWin.Max().x - 28.f;
    const float colW = (x1 - x0 - 10.f) * 0.5f;
    const int col = index % 2, row = index / 2;
    return ui::Rect(x0 + col * (colW + 10.f), kWin.y + kHead + 14.f + row * (kRowH + kRowGap),
                    colW, kRowH);
}

// Mouse rests on "Auto Shield Break" (index 2) so the preview reflects the Grim Client reference.
inline ui::Vec2 HoverPoint() {
    ui::Rect r = RowRect(2);
    return ui::Vec2(r.x + 38.f, r.y + 6.f);
}

// Vivid game-sky backdrop: gradient, soft light blobs, Minecraft terrain.
inline void Backdrop(ui::Context& ctx, float w, float h) {
    // 1) Twilight night sky gradient
    ui::RectStyle sky;
    sky.fill = ui::Color::Hex(0x38186D);
    sky.fillEnd = ui::Color::Hex(0x130728);
    sky.gradientDir = ui::Vec2(0.2f, 1.f);
    ctx.DrawRect(ui::Rect(0, 0, w, h), sky);

    // 2) Ambient glow blobs
    auto blob = [&](float cx, float cy, float r, ui::Color c, float op) {
        ui::RectStyle b;
        b.fill = ui::Color::Transparent();
        b.cornerRadii = ui::Vec4(r);
        b.shadowColor = c;
        b.shadowBlur = r * 1.5f;
        b.shadowOpacity = op;
        ctx.DrawRect(ui::Rect(cx - r, cy - r, r * 2.f, r * 2.f), b);
    };
    blob(w * 0.88f, h * 0.20f, 120.f, ui::Color::Hex(0x9E52FF), 0.45f);
    blob(w * 0.10f, h * 0.28f, 130.f, ui::Color::Hex(0x4A2A9A), 0.35f);

    // 3) Glowing magenta crystal / facet on the right edge
    {
        ui::RectStyle facet;
        facet.fill = ui::Color::Hex(0xC238FF, 180);
        facet.fillEnd = ui::Color::Hex(0x8B1EDB, 220);
        facet.gradientDir = ui::Vec2(1.f, 1.f);
        facet.cornerRadii = ui::Vec4(4.f);
        facet.shadowColor = ui::Color::Hex(0xE156FF);
        facet.shadowBlur = 38.f;
        facet.shadowOpacity = 0.7f;
        ctx.DrawRect(ui::Rect(w - 70.f, h * 0.38f, 85.f, 65.f), facet);
    }

    // 4) Minecraft block terrain at the bottom
    const float groundY = h - 56.f;

    // Grass blocks across the bottom
    for (float bx = 0; bx < w + 60.f; bx += 56.f) {
        // Dirt side
        ui::RectStyle dirt;
        dirt.fill = ui::Color::Hex(0x765338);
        dirt.fillEnd = ui::Color::Hex(0x5E412A);
        dirt.gradientDir = ui::Vec2(0.f, 1.f);
        ctx.DrawRect(ui::Rect(bx, groundY, 56.f, 56.f), dirt);

        // Grass top
        ui::RectStyle grass;
        grass.fill = ui::Color::Hex(0x5B8E31);
        grass.fillEnd = ui::Color::Hex(0x4D7928);
        grass.gradientDir = ui::Vec2(0.f, 1.f);
        ctx.DrawRect(ui::Rect(bx, groundY, 56.f, 16.f), grass);

        // Block grid lines
        ctx.DrawLine(ui::Vec2(bx, groundY), ui::Vec2(bx, h), 1.f, ui::Color(0, 0, 0, 45));
    }

    // Sand/birch blocks on the right
    for (int gy = 0; gy < 4; ++gy) {
        for (int gx = 0; gx < 5; ++gx) {
            if (gx + gy < 2) continue;
            const float sx = w - 56.f * (float)(gx + 1) + 24.f;
            const float sy = h - 56.f * (float)(gy + 1) + 12.f;
            ui::RectStyle sand;
            sand.fill = ui::Color::Hex(0xDCCFA6);
            sand.fillEnd = ui::Color::Hex(0xC2B389);
            sand.gradientDir = ui::Vec2(0.f, 1.f);
            ctx.DrawRect(ui::Rect(sx, sy, 54.f, 54.f), sand);
            ctx.DrawLine(ui::Vec2(sx, sy), ui::Vec2(sx + 54.f, sy), 1.f, ui::Color(0, 0, 0, 35));
            ctx.DrawLine(ui::Vec2(sx, sy), ui::Vec2(sx, sy + 54.f), 1.f, ui::Color(0, 0, 0, 35));
        }
    }
}

inline void Scene(ui::Context& ctx, aris_demo::DemoState&) {
    using namespace ui;
    const kit::Palette& pal = kit::Aurora();
    Backdrop(ctx, ctx.size().x, ctx.size().y);
    kit::GlassPanel(ctx, kWin, pal, 22.f);

    const float px = kWin.x, py = kWin.y;

    // --- brand (Grim Client horned crest logo) ---
    {
        DrawIcon(ctx, Icon::GrimCrest, Rect(px + 24.f, py + 22.f, 38.f, 38.f),
                 Color::Hex(0xE2DDEC), 1.75f);
        ctx.DrawText(Vec2(px + 72.f, py + 23.f), "Grim Client", pal.text, 16.5f, nullptr, 0.45f);
        ctx.DrawText(Vec2(px + 72.f, py + 43.f), "Version: 4.2", pal.textDim, 11.5f, nullptr, -0.05f);
    }

    // --- sidebar ---
    struct Nav { Icon icon; const char* label; };
    static const Nav features[] = {{Icon::Combat, "Combat"}, {Icon::Movement, "Movement"},
                                   {Icon::Player, "Player"}, {Icon::Visuals, "Visuals"},
                                   {Icon::Settings, "Client"}};
    static const Nav others[] = {{Icon::Friends, "Friends"}, {Icon::Configs, "Configs"},
                                 {Icon::Hud, "Hud"}};
    static int selected = 0;   // preview: Combat
    float y = py + 86.f;
    kit::SectionTitle(ctx, pal, Vec2(px + 28.f, y), "FEATURES");
    y += 22.f;
    int idx = 0;
    for (const Nav& n : features) {
        if (kit::NavItem(ctx, pal, n.label, Rect(px + 16.f, y, kSide - 32.f, 42.f), n.icon,
                         n.label, selected == idx))
            selected = idx;
        ++idx;
        y += 48.f;
    }
    y += 12.f;
    kit::SectionTitle(ctx, pal, Vec2(px + 28.f, y), "OTHER");
    y += 22.f;
    for (const Nav& n : others) {
        if (kit::NavItem(ctx, pal, n.label, Rect(px + 16.f, y, kSide - 32.f, 42.f), n.icon,
                         n.label, selected == idx))
            selected = idx;
        ++idx;
        y += 48.f;
    }

    // user footer (profile outline icon + name + UID)
    {
        const float fy = kWin.Max().y - 56.f;
        DrawIcon(ctx, Icon::Player, Rect(px + 24.f, fy + 4.f, 24.f, 24.f), pal.textDim, 1.7f);
        ctx.DrawText(Vec2(px + 58.f, fy + 4.f), "vvv", pal.text, 13.5f, nullptr, 0.15f);
        ctx.DrawText(Vec2(px + 58.f, fy + 22.f), "UID: 1", pal.textDim, 11.f);
    }

    // --- header ---
    {
        const float hx = px + kSide + 28.f, cy = py + kHead * 0.5f + 4.f;
        DrawIcon(ctx, Icon::Combat, Rect(hx, cy - 11.f, 22.f, 22.f), Color::Hex(0xE0DAEE), 1.8f);
        ctx.DrawText(Vec2(hx + 34.f, cy - 10.f), "Combat", pal.text, 17.f, nullptr, 0.45f);
        const float tw = ctx.MeasureText("Combat", 17.f).x;
        ctx.DrawCircle(Vec2(hx + 34.f + tw + 14.f, cy + 1.f), 2.f, pal.textFaint);
        ctx.DrawText(Vec2(hx + 34.f + tw + 26.f, cy - 7.5f), "Automatically attacks entities",
                     pal.textDim, 12.5f);
        kit::IconButton(ctx, pal, "search", Rect(kWin.Max().x - 28.f - 36.f, cy - 18.f, 36.f, 36.f),
                        Icon::Search);
    }

    // --- module list ---
    Module* m = Modules();
    for (int i = 0; i < 16; ++i)
        kit::ModuleRow(ctx, pal, m[i].name, RowRect(i), m[i].name, m[i].desc, &m[i].on, /*withSwitch=*/false);

    // --- mouse cursor ---
    const Vec2 mp = ctx.mouse();
    if (mp.x > 0.f && mp.y > 0.f) {
        DrawIcon(ctx, Icon::Cursor, Rect(mp.x + 1.f, mp.y + 1.f, 18.f, 18.f), Color(0, 0, 0, 210), 2.2f);
        DrawIcon(ctx, Icon::Cursor, Rect(mp.x, mp.y, 18.f, 18.f), Color::White(), 1.6f);
    }
}

} // namespace aurora
