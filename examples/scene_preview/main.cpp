// aris-ui scene previewer — headless UI snapshot tool for agents & CI.
//
// Renders named UI scenes into an offscreen GLFW window (no visible window,
// no user interaction) and writes one PNG per scene. Designed so a coding
// agent can *see* the result of a change:
//
//   aris_scene_preview --list
//   aris_scene_preview --all --out previews
//   aris_scene_preview --scene panel,sliders --out previews --warmup 8
//
// Exit code 0 on success; a scene name that doesn't exist is an error.
// Easing (hover/press) settles over `--warmup` frames before the snapshot.
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "common/AppSupport.hpp"
#include "ui/aris_ui.hpp"
#include "atlas_generator/atlas_gen.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "stb_image_write.h"

namespace {

struct Scene {
    const char* name;
    const char* description;
    // Builds the UI for one frame. `warmup`-many frames are rendered before
    // the snapshot so eased state (hover/press sliders) settles.
    void (*build)(ui::Context& ctx, aris_demo::DemoState& st);
    // Optional per-frame simulated input (mouse position / button state).
    void (*inject)(ui::Context& ctx) = nullptr;
};

ui::Vec2 ScenePanelPos(const ui::Context&) { return ui::Vec2(28.f, 28.f); }

// --- scenes ----------------------------------------------------------------

void ScenePanel(ui::Context& ctx, aris_demo::DemoState& st) {
    aris_demo::BuildDemoUi(ctx, st);   // full canonical demo
}

void SceneButtons(ui::Context& ctx, aris_demo::DemoState&) {
    const ui::Theme& th = ui::Theme::Default();
    ui::PanelStyle ps = ui::PanelStyle::FromTheme(th, 16.f, "Buttons");
    ctx.BeginPanel("p", ui::Rect(28, 28, 300, 180), ps);
    ui::Button(ctx, "Click me");
    ui::Button(ctx, "Accent action");
    ctx.EndPanel();
}

void SceneButtonsHover(ui::Context& ctx, aris_demo::DemoState&) {
    const ui::Theme& th = ui::Theme::Default();
    ui::PanelStyle ps = ui::PanelStyle::FromTheme(th, 16.f, "Buttons (hover)");
    ctx.BeginPanel("p", ui::Rect(28, 28, 300, 180), ps);
    ui::Button(ctx, "Click me");      // mouse rests on this one (inject below)
    ui::Button(ctx, "Accent action");
    ctx.EndPanel();
}

void SceneButtonsPress(ui::Context& ctx, aris_demo::DemoState&) {
    const ui::Theme& th = ui::Theme::Default();
    ui::PanelStyle ps = ui::PanelStyle::FromTheme(th, 16.f, "Buttons (pressed)");
    ctx.BeginPanel("p", ui::Rect(28, 28, 300, 180), ps);
    ui::Button(ctx, "Click me");      // held down via inject below
    ui::Button(ctx, "Accent action");
    ctx.EndPanel();
}

void SceneToggles(ui::Context& ctx, aris_demo::DemoState&) {
    const ui::Theme& th = ui::Theme::Default();
    ui::PanelStyle ps = ui::PanelStyle::FromTheme(th, 16.f, "Toggles");
    ctx.BeginPanel("p", ui::Rect(28, 28, 300, 170), ps);
    bool on = true, off = false;
    ui::Toggle(ctx, "Enabled", &on);
    ui::Toggle(ctx, "Disabled", &off);
    ctx.EndPanel();
}

void SceneSliders(ui::Context& ctx, aris_demo::DemoState&) {
    const ui::Theme& th = ui::Theme::Default();
    ui::PanelStyle ps = ui::PanelStyle::FromTheme(th, 16.f, "Sliders");
    ctx.BeginPanel("p", ui::Rect(28, 28, 300, 250), ps);
    float a = 0.f, b = 0.45f, c = 1.f;
    ui::Slider(ctx, "Zero", &a, 0.f, 1.f);
    ui::Slider(ctx, "Mid", &b, 0.f, 1.f);
    ui::Slider(ctx, "Full", &c, 0.f, 1.f);
    ctx.EndPanel();
}

void SceneText(ui::Context& ctx, aris_demo::DemoState&) {
    const ui::Theme& th = ui::Theme::Default();
    ui::PanelStyle ps = ui::PanelStyle::FromTheme(th, 16.f, "Text");
    ctx.BeginPanel("p", ui::Rect(28, 28, 420, 330), ps);
    float y = 84.f;
    for (float size = 11.f; size <= 26.f; size += 3.f) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "MSDF text %.0f px — crisp at any size", (double)size);
        ctx.DrawText(ui::Vec2(46.f, y), buf, th.text, size);
        y += size + 12.f;
    }
    y += 6.f;
    ctx.DrawText(ui::Vec2(46.f, y), "dim secondary text", th.textDim, 13.f);
    ctx.DrawText(ui::Vec2(46.f, y + 22.f), "accent colored text", th.accent, 13.f);
    ctx.EndPanel();
}

void SceneCard(ui::Context& ctx, aris_demo::DemoState&) {
    const ui::Theme& th = ui::Theme::Default();
    ui::PanelStyle ps = ui::PanelStyle::FromTheme(th, 16.f, "Preview card");
    ctx.BeginPanel("p", ui::Rect(28, 28, 420, 300), ps);
    float x = 46.f;
    for (float radius : {0.f, 10.f, 20.f}) {
        ui::RectStyle st;
        st.fill = ui::Color::Hex(0x1C2128, 216);
        st.cornerRadii = ui::Vec4(radius);
        st.borderColor = th.controlBorder;
        st.borderWidth = 1.f;
        st.shadowOffset = ui::Vec2(0.f, 10.f);
        st.shadowBlur = 24.f;
        st.shadowOpacity = 0.45f;
        ctx.DrawRect(ui::Rect(x, 56.f, 110.f, 210.f), st);
        x += 118.f;
    }
    ctx.EndPanel();
}

// ---------------------------------------------------------------------------
// Mock-game overlays (aoko-client style): arraylist, nametags, ESP, ClickGUI.
// These draw on absolute coordinates — overlays don't need retained layout.
// ---------------------------------------------------------------------------

constexpr ui::Color kAccent  = ui::Color::Hex(0x7FA8CC);
constexpr ui::Color kAccent2 = ui::Color::Hex(0xA9C6DE);
constexpr ui::Color kAccent3 = ui::Color::Hex(0x5F87AC);
constexpr ui::Color kHair    = ui::Color::Hex(0x2A3644);

// Filled rounded rect shortcut.
void Fill(ui::Context& ctx, ui::Rect r, ui::Color c, float radius = 0.f,
          ui::Color border = ui::Color::Transparent(), float bw = 0.f) {
    ui::RectStyle st;
    st.fill = c;
    st.cornerRadii = ui::Vec4(radius);
    st.borderColor = border;
    st.borderWidth = bw;
    ctx.DrawRect(r, st);
}

// Night-forest backdrop: sky, moon, stars, treeline, blocky ground, ruin.
void DrawWorld(ui::Context& ctx, float w, float h) {
    Fill(ctx, ui::Rect(0, 0, w, h), ui::Color::Hex(0x0B1220));   // sky
    Fill(ctx, ui::Rect(w * 0.80f, h * 0.16f, 34, 34), ui::Color::Hex(0xD9E6F2, 235), 17.f);
    // stars
    const int starCount = std::min(90, (int)(w * h / 9000.f));
    for (int i = 0; i < starCount; ++i) {
        float sx = std::fmod((i * 137.f) + 31.f, w);
        float sy = std::fmod((i * 89.f) + 17.f, h * 0.55f);
        Fill(ctx, ui::Rect(sx, sy, 2, 2), ui::Color::Hex(0x8FA5BD, 130), 1.f);
    }
    const float horizon = h * 0.60f;
    // treeline: overlapping dark canopy blobs
    for (int i = 0; i < (int)(w / 55.f) + 2; ++i) {
        float cx = (float)i * 55.f - 20.f;
        float r  = 26.f + std::fmod(i * 37.f, 22.f);
        Fill(ctx, ui::Rect(cx, horizon - r * 1.5f, r * 2.2f, r * 1.6f),
             ui::Color::Hex(0x0E1B15), r);
    }
    // ground with block texture
    Fill(ctx, ui::Rect(0, horizon, w, h - horizon), ui::Color::Hex(0x0C1410));
    for (int gy = 0; horizon + gy * 34.f < h; ++gy)
        for (int gx = 0; gx * 38.f < w; ++gx)
            Fill(ctx, ui::Rect(gx * 38.f + 2.f, horizon + gy * 34.f + 4.f, 34, 30),
                 ui::Color::Hex(0x111A13, (std::uint8_t)(104 - gy * 14)), 4.f);
    // stone ruin silhouette on the left
    for (int i = 0; i < 4; ++i)
        Fill(ctx, ui::Rect(46.f, horizon - 40.f - i * 34.f, 52.f, 32.f),
             ui::Color::Hex(0x18202A), 3.f, kHair, 1.f);
    Fill(ctx, ui::Rect(104.f, horizon - 76.f, 30.f, 70.f), ui::Color::Hex(0x161E27), 3.f, kHair, 1.f);
}

struct MockPlayer {
    float x, yFeet;         // position
    const char* name;
    const char* sub;        // secondary nametag line (nullptr = none)
    float hp;               // 0..1
    bool target;
};

ui::Rect PlayerBounds(const MockPlayer& p) { return ui::Rect(p.x - 13.f, p.yFeet - 54.f, 26.f, 54.f); }

void DrawPlayer(ui::Context& ctx, const MockPlayer& p) {
    const ui::Rect b = PlayerBounds(p);
    Fill(ctx, ui::Rect(b.x + 5.f, b.y + 32.f, 16.f, 22.f), ui::Color::Hex(0x070C12), 4.f, kHair, 1.f);
    Fill(ctx, ui::Rect(b.x + 3.f, b.y + 15.f, 20.f, 19.f), ui::Color::Hex(0x0A1119), 5.f, kHair, 1.f);
    Fill(ctx, ui::Rect(b.x + 7.f, b.y, 12.f, 13.f), ui::Color::Hex(0x070C12), 6.f, kHair, 1.f);
}

// 2D box ESP with a health bar on the left edge.
void DrawEsp(ui::Context& ctx, const MockPlayer& p) {
    ui::Rect b = PlayerBounds(p).Inset(-7.f);
    ui::Color c = p.target ? kAccent2 : kAccent;
    ui::RectStyle box;
    box.fill = ui::Color::Transparent();
    box.borderColor = c;
    box.borderWidth = 1.5f;
    box.cornerRadii = ui::Vec4(3.f);
    ctx.DrawRect(b, box);

    // health track + fill
    ui::Rect hb(b.x - 8.f, b.y + 1.f, 3.f, b.h - 2.f);
    Fill(ctx, hb, ui::Color::Hex(0x1A222C), 1.5f);
    float hh = hb.h * p.hp;
    Fill(ctx, ui::Rect(hb.x, hb.Max().y - hh, hb.w, hh),
         p.target ? kAccent2 : kAccent, 1.5f);
}

// Floating nametag: name pill above the head, optional sub-pill under it.
void DrawNametag(ui::Context& ctx, const MockPlayer& p) {
    ui::Rect b = PlayerBounds(p);
    const float fs = 13.f;
    const ui::Vec2 ts = ctx.MeasureText(p.name, fs);
    const float tw = ts.x + 20.f;
    const float cx = b.Center().x;

    ui::Rect tag(cx - tw * 0.5f, b.y - 36.f, tw, 24.f);
    Fill(ctx, tag, ui::Color::Hex(0x05080C, 235), 6.f, kHair, 1.f);
    ctx.DrawText(ui::Vec2(tag.x + 10.f, tag.y + (tag.h - ts.y) * 0.5f),
                 p.name, ui::Color::Hex(0xE6EDF3), fs, &tag);

    if (p.sub) {
        const float fs2 = 11.f;
        const ui::Vec2 ts2 = ctx.MeasureText(p.sub, fs2);
        const float tw2 = ts2.x + 16.f;
        ui::Rect sub(cx - tw2 * 0.5f, tag.Max().y + 4.f, tw2, 20.f);
        Fill(ctx, sub, ui::Color::Hex(0x05080C, 210), 5.f, kHair, 1.f);
        ctx.DrawText(ui::Vec2(sub.x + 8.f, sub.y + (sub.h - ts2.y) * 0.5f),
                     p.sub, ui::Color::Hex(0x9DB0C4), fs2, &sub);
    }
}

// Top-right module arraylist: right-aligned labels + accent bars.
void DrawArraylist(ui::Context& ctx, float w, float y0) {
    struct Mod { const char* name; ui::Color c; };
    const Mod mods[] = {
        {"ESP",         kAccent2},
        {"Nametags",    kAccent},
        {"Chest ESP",   kAccent3},
        {"AutoClicker", kAccent},
        {"Reach",       ui::Color::Hex(0x8FA9C4)},
        {"Click GUI",   kAccent3},
    };
    const float fs = 14.f, rowH = 24.f;
    float y = y0;
    for (const Mod& m : mods) {
        const ui::Vec2 ts = ctx.MeasureText(m.name, fs);
        const float right = w - 16.f;
        ctx.DrawText(ui::Vec2(right - 11.f - ts.x, y + (rowH - ts.y) * 0.5f),
                     m.name, ui::Color::Hex(0xE6EDF3), fs);
        Fill(ctx, ui::Rect(right - 3.f, y + 5.f, 3.f, rowH - 10.f), m.c, 1.5f);
        y += rowH;
    }
}

// Top-left watermark pill.
void DrawWatermark(ui::Context& ctx, float x, float y) {
    const char* t = "aris-ui";
    const ui::Vec2 ts = ctx.MeasureText(t, 14.f);
    ui::Rect r(x, y, ts.x + 62.f, 28.f);
    Fill(ctx, r, ui::Color::Hex(0x060A10, 225), 7.f, kHair, 1.f);
    ctx.DrawText(ui::Vec2(x + 12.f, y + (28.f - ts.y) * 0.5f), t, ui::Color::Hex(0xE6EDF3), 14.f);
    ctx.DrawText(ui::Vec2(x + 12.f + ts.x + 10.f, y + (28.f - ts.y) * 0.5f + 1.f),
                 "0.1", ui::Color::Hex(0x7285A0), 13.f);
}

// Players positioned relative to the viewport so any --width/--height works.
std::vector<MockPlayer> PlayersFor(float w, float h) {
    return {
        {w * 0.27f, h * 0.89f, "Player_042",  nullptr,           0.65f, false},
        {w * 0.54f, h * 0.83f, "aris",        "holding: Gapple", 0.90f, true},
        {w * 0.77f, h * 0.90f, "xX_Ghost_Xx", nullptr,           0.30f, false},
    };
}

void SceneHud(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    DrawWorld(ctx, w, h);

    // hotbar, bottom-center: 9 slots, active slot highlighted
    const float slot = 30.f, gap = 3.f, n = 9;
    const float hbW = n * slot + (n - 1) * gap;
    const float hx = (w - hbW) * 0.5f, hy = h - slot - 18.f;
    Fill(ctx, ui::Rect(hx - 6.f, hy - 6.f, hbW + 12.f, slot + 12.f),
         ui::Color::Hex(0x05080C, 200), 8.f, kHair, 1.f);
    for (int i = 0; i < n; ++i) {
        ui::Rect r(hx + i * (slot + gap), hy, slot, slot);
        if (i == 2) {
            Fill(ctx, r, ui::Color::Hex(0x1A2330, 230), 5.f, kAccent2, 1.5f);
            Fill(ctx, ui::Rect(r.x + 7.f, r.y + 10.f, 16.f, 12.f), kAccent, 2.f);
        } else {
            Fill(ctx, r, ui::Color::Hex(0x0D141D, 200), 5.f, kHair, 1.f);
        }
    }

    for (const MockPlayer& p : PlayersFor(w, h)) {
        DrawPlayer(ctx, p);
        DrawEsp(ctx, p);
        DrawNametag(ctx, p);
    }
    DrawArraylist(ctx, w, 18.f);
    DrawWatermark(ctx, 16.f, 16.f);
}

void SceneArraylist(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    DrawWorld(ctx, w, h);
    DrawArraylist(ctx, w, 18.f);
    DrawWatermark(ctx, 16.f, 16.f);
}

void SceneNametags(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    DrawWorld(ctx, w, h);
    for (const MockPlayer& p : PlayersFor(w, h)) {
        DrawPlayer(ctx, p);
        DrawNametag(ctx, p);
    }
}

void SceneEsp(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    DrawWorld(ctx, w, h);
    for (const MockPlayer& p : PlayersFor(w, h)) {
        DrawPlayer(ctx, p);
        DrawEsp(ctx, p);
    }
}

// --- mock ClickGUI ----------------------------------------------------------

void MockToggle(ui::Context& ctx, float x, float y, bool on) {
    ui::Rect track(x, y, 40.f, 22.f);
    ui::RectStyle st;
    st.cornerRadii = ui::Vec4(11.f);
    st.fill = on ? kAccent : ui::Color::Hex(0x232C38);
    if (!on) { st.borderColor = kHair; st.borderWidth = 1.f; }
    ctx.DrawRect(track, st);
    const float knob = 18.f;
    const float kx = on ? track.Max().x - knob - 2.f : track.x + 2.f;
    ui::RectStyle ks;
    ks.fill = ui::Color::White();
    ks.cornerRadii = ui::Vec4(knob * 0.5f);
    ks.shadowOffset = ui::Vec2(0.f, 1.f);
    ks.shadowBlur = 3.f;
    ks.shadowOpacity = 0.25f;
    ctx.DrawRect(ui::Rect(kx, y + 2.f, knob, knob), ks);
}

void MockSlider(ui::Context& ctx, float x, float y, float w,
                const char* label, const char* val, float frac) {
    const float fs = 13.f;
    ctx.DrawText(ui::Vec2(x, y), label, ui::Color::Hex(0xE6EDF3), fs);
    const ui::Vec2 vs = ctx.MeasureText(val, fs);
    ctx.DrawText(ui::Vec2(x + w - vs.x, y), val, ui::Color::Hex(0x8B9DB3), fs);

    const float ty = y + 30.f, th2 = 5.f;
    ui::Rect track(x, ty, w, th2);
    Fill(ctx, track, ui::Color::Hex(0x232C38), 2.5f);
    const float fx = x + w * frac;
    Fill(ctx, ui::Rect(x, ty, fx - x, th2), kAccent, 2.5f);
    ui::RectStyle knob;
    knob.fill = ui::Color::White();
    knob.cornerRadii = ui::Vec4(6.f);
    knob.shadowOffset = ui::Vec2(0.f, 1.f);
    knob.shadowBlur = 3.f;
    knob.shadowOpacity = 0.3f;
    ctx.DrawRect(ui::Rect(fx - 6.f, ty - 3.5f, 12.f, 12.f), knob);
}

void MockCheckbox(ui::Context& ctx, float x, float y, const char* label, bool checked) {
    const float fs = 13.f;
    if (checked) {
        Fill(ctx, ui::Rect(x, y, 15.f, 15.f), kAccent, 4.f);
        const ui::Vec2 ts = ctx.MeasureText("\xE2\x9C\x93", 12.f);   // ✓
        ctx.DrawText(ui::Vec2(x + (15.f - ts.x) * 0.5f, y + (15.f - ts.y) * 0.5f + 1.f),
                     "\xE2\x9C\x93", ui::Color::Hex(0x0B0F14), 12.f);
    } else {
        Fill(ctx, ui::Rect(x, y, 15.f, 15.f), ui::Color::Hex(0x1A222C), 4.f, kHair, 1.f);
    }
    ctx.DrawText(ui::Vec2(x + 24.f, y - 1.f), label, ui::Color::Hex(0xE6EDF3), fs);
}

void MockBindChip(ui::Context& ctx, float right, float y, const char* text) {
    const float fs = 12.f;
    const ui::Vec2 ts = ctx.MeasureText(text, fs);
    ui::Rect r(right - ts.x - 20.f, y, ts.x + 20.f, 24.f);
    Fill(ctx, r, ui::Color::Hex(0x151D28), 6.f, kHair, 1.f);
    ctx.DrawText(ui::Vec2(r.x + 10.f, y + (24.f - ts.y) * 0.5f), text,
                 ui::Color::Hex(0xC9D6E3), fs);
}

void MockCard(ui::Context& ctx, ui::Rect r) {
    ui::RectStyle st;
    st.fill = ui::Color::Hex(0x111722, 250);
    st.cornerRadii = ui::Vec4(10.f);
    st.borderColor = kHair;
    st.borderWidth = 1.f;
    st.shadowOffset = ui::Vec2(0.f, 8.f);
    st.shadowBlur = 20.f;
    st.shadowOpacity = 0.35f;
    ctx.DrawRect(r, st);
}

void SceneClickGui(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    Fill(ctx, ui::Rect(0, 0, w, h), ui::Color::Hex(0x0B1220));

    // GUI frame
    ui::PanelStyle ps;
    ps.bg = ui::Color::Hex(0x0C1118, 248);
    ps.border = kHair;
    ps.borderWidth = 1.f;
    ps.cornerRadii = ui::Vec4(10.f);
    ps.shadowOffset = ui::Vec2(0.f, 14.f);
    ps.shadowBlur = 36.f;
    ps.shadowOpacity = 0.5f;
    ctx.BeginPanel("gui", ui::Rect(30, 40, 500, 600), ps);
    ctx.EndPanel();

    // --- sidebar ---
    const float sx = 48.f;
    ctx.DrawText(ui::Vec2(sx, 58.f), "aris-ui", ui::Color::Hex(0xE6EDF3), 16.f);
    ctx.DrawText(ui::Vec2(sx, 82.f), "v0.1 preview", ui::Color::Hex(0x7285A0), 11.f);
    const char* cats[] = {"Combat", "Render", "Utility", "Stats", "Settings"};
    float cy = 112.f;
    for (int i = 0; i < 5; ++i) {
        const bool active = (i == 0);
        ui::Rect r(sx, cy, 122.f, 38.f);
        if (active) {
            Fill(ctx, r, ui::Color::Hex(0x1A2330), 8.f, kHair, 1.f);
            Fill(ctx, ui::Rect(sx + 8.f, cy + 9.f, 3.f, 20.f), kAccent2, 1.5f);
            ctx.DrawText(ui::Vec2(sx + 22.f, cy + (38.f - 15.f) * 0.5f + 1.f),
                         cats[i], ui::Color::Hex(0xE6EDF3), 15.f);
        } else {
            ctx.DrawText(ui::Vec2(sx + 22.f, cy + (38.f - 14.f) * 0.5f + 1.f),
                         cats[i], ui::Color::Hex(0x7285A0), 14.f);
        }
        cy += 46.f;
    }

    // --- module cards ---
    const float mx = 188.f, mw = 324.f;

    // Card 1: AutoClicker (on, bound)
    MockCard(ctx, ui::Rect(mx, 58.f, mw, 276.f));
    ctx.DrawText(ui::Vec2(mx + 16.f, 74.f), "AutoClicker", ui::Color::Hex(0xE6EDF3), 15.f);
    MockBindChip(ctx, mx + mw - 16.f, 72.f, "Bind: `");
    MockToggle(ctx, mx + mw - 16.f - 66.f - 50.f, 72.f, true);
    MockSlider(ctx, mx + 16.f, 112.f, mw - 32.f, "Min CPS", "14", 0.56f);
    MockSlider(ctx, mx + 16.f, 158.f, mw - 32.f, "Max CPS", "18", 0.72f);
    ctx.DrawText(ui::Vec2(mx + 16.f, 204.f), "Randomization", ui::Color::Hex(0xE6EDF3), 13.f);
    ctx.DrawText(ui::Vec2(mx + mw - 16.f - 62.f, 204.f), "Advanced", ui::Color::Hex(0x7285A0), 12.f);
    MockSlider(ctx, mx + 16.f, 228.f, mw - 32.f, "", "0.93", 0.93f);
    MockCheckbox(ctx, mx + 16.f, 274.f, "Click in Chests", true);
    MockCheckbox(ctx, mx + 16.f + 130.f, 274.f, "Pause on break", true);

    // Card 2: Right Click (off, unbound)
    MockCard(ctx, ui::Rect(mx, 348.f, mw, 232.f));
    ctx.DrawText(ui::Vec2(mx + 16.f, 364.f), "Right Click", ui::Color::Hex(0xE6EDF3), 15.f);
    MockBindChip(ctx, mx + mw - 16.f, 362.f, "Bind: Unbound");
    MockToggle(ctx, mx + mw - 16.f - 110.f - 50.f, 362.f, false);
    MockSlider(ctx, mx + 16.f, 404.f, mw - 32.f, "Right Min CPS", "14", 0.56f);
    MockSlider(ctx, mx + 16.f, 450.f, mw - 32.f, "Right Max CPS", "15", 0.62f);
    MockCheckbox(ctx, mx + 16.f, 494.f, "Only when holding block", false);

    // footer
    ctx.DrawText(ui::Vec2(sx, 600.f), "up to date", ui::Color::Hex(0x7285A0), 11.f);
    ctx.DrawText(ui::Vec2(sx, 616.f), "current v0.1", ui::Color::Hex(0x4E5D70), 10.f);
}

// ---------------------------------------------------------------------------

// Mouse position helpers: button #1 sits at panel content origin + row gap.
// Panel (28,28) + padding 16 + title row → first widget ≈ (44, 92) sized
// 268x34. Derived from PanelStyle::FromTheme(16) geometry.
constexpr ui::Vec2 kFirstWidgetCenter(178.f, 109.f);

std::vector<std::string> SplitList(const std::string& csv) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : csv) {
        if (c == ',') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

const Scene kScenes[] = {
    {"panel",          "full canonical demo panel (all MVP widgets)",   ScenePanel},
    {"buttons",        "two accent buttons, idle state",                SceneButtons},
    {"buttons-hover",  "first button hovered (eased hover state)",      SceneButtonsHover, [](ui::Context& c) {
        c.ProcessInput(ui::InputEvent{ui::MouseMoveEvent{kFirstWidgetCenter}});
    }},
    {"buttons-press",  "first button held down (eased press state)",    SceneButtonsPress, [](ui::Context& c) {
        c.ProcessInput(ui::InputEvent{ui::MouseMoveEvent{kFirstWidgetCenter}});
        c.ProcessInput(ui::InputEvent{ui::MouseButtonEvent{
            ui::MouseButton::Left, true, kFirstWidgetCenter}});
    }},
    {"toggles",        "toggle switches, on + off",                     SceneToggles},
    {"sliders",        "sliders at 0 / mid / full",                     SceneSliders},
    {"text",           "MSDF text at multiple sizes and colors",        SceneText},
    {"card",           "SDF preview cards at radius 0/10/20",           SceneCard},
    {"hud",            "full overlay HUD: arraylist + ESP + nametags",  SceneHud},
    {"arraylist",      "module arraylist (top-right)",                  SceneArraylist},
    {"nametags",       "floating nametags with sub-tags",               SceneNametags},
    {"esp",            "2D box ESP with health bars",                   SceneEsp},
    {"clickgui",       "mock ClickGUI: sidebar + module cards",         SceneClickGui},
};

// --- harness ----------------------------------------------------------------

void PrintUsage() {
    std::printf(
        "aris scene previewer — headless UI snapshots\n\n"
        "usage:\n"
        "  aris_scene_preview --list\n"
        "  aris_scene_preview [--scene NAME[,NAME...]] [--all] [--out DIR]\n"
        "                     [--width W] [--height H] [--warmup N] [--font TTF]\n\n"
        "options:\n"
        "  --list            print scene names + descriptions, one per line\n"
        "  --scene NAMES     comma-separated scenes to render (default: all)\n"
        "  --all             render every scene\n"
        "  --out DIR         output directory (default: previews)\n"
        "  --width/--height  framebuffer size (default 560x720)\n"
        "  --warmup N        frames rendered before snapshot for easing (default 10)\n"
        "  --font TTF        font file (default: auto-discover system font)\n"
        "\noutput: DIR/<scene>.png (+ one 'scene\\tpath' line on stdout)\n");
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> want;
    std::string outDir = "previews";
    std::string fontArg;
    int width = 560, height = 720, warmup = 10;
    bool listOnly = false, all = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* def = "") -> std::string {
            return (i + 1 < argc) ? argv[++i] : def;
        };
        if (a == "--list") listOnly = true;
        else if (a == "--all") all = true;
        else if (a == "--scene") {
            for (const auto& s : SplitList(next())) want.push_back(s);
        } else if (a == "--out") outDir = next();
        else if (a == "--width") width = std::atoi(next("560").c_str());
        else if (a == "--height") height = std::atoi(next("720").c_str());
        else if (a == "--warmup") warmup = std::atoi(next("10").c_str());
        else if (a == "--font") fontArg = next();
        else if (a == "--help" || a == "-h") { PrintUsage(); return 0; }
        else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); PrintUsage(); return 2; }
    }

    if (listOnly) {
        for (const Scene& s : kScenes)
            std::printf("%s\t%s\n", s.name, s.description);
        return 0;
    }

    if (!all && want.empty()) want.assign({"panel"});   // sensible default

    // validate scene names up front (fail fast for agents)
    for (const std::string& w : want) {
        bool found = false;
        for (const Scene& s : kScenes) found |= (w == s.name);
        if (!found) {
            std::fprintf(stderr, "unknown scene '%s' — use --list\n", w.c_str());
            return 2;
        }
    }

    const std::string fontPath = !fontArg.empty() ? fontArg : aris_demo::FindSystemFont();
    if (fontPath.empty()) {
        std::fprintf(stderr, "no system font found; pass one: --font <font.ttf>\n");
        return 1;
    }
    std::string err;
    if (!aris_demo::BootstrapAtlas(fontPath, "atlas/ui", err)) {
        std::fprintf(stderr, "atlas generation failed: %s\n", err.c_str());
        return 1;
    }

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);              // fully headless
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(width, height, "aris scene preview", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if (gladLoadGL((GLADloadfunc)glfwGetProcAddress) == 0) return 1;
    glfwSwapInterval(0);

    ui::FontAtlas font;   // uploads a texture — must happen on a live GL context
    if (!font.Load("atlas/ui.png", "atlas/ui.json")) return 1;

    std::filesystem::create_directories(outDir);

    int failures = 0;
    for (const Scene& s : kScenes) {
        bool wanted = all;
        for (const std::string& w : want) wanted |= (w == s.name);
        if (!wanted) continue;

        ui::Context ctx;   // fresh context per scene: no state bleed
        ctx.SetTheme(ui::Theme::Default());
        ctx.SetFont(&font);

        for (int f = 0; f < warmup; ++f) {
            glClearColor(0.08f, 0.08f, 0.1f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);   // fresh buffer each frame (no ghosting)
            if (s.inject) s.inject(ctx);   // simulated input settles over frames
            ctx.BeginFrame(1.f / 60.f, (float)width, (float)height);
            aris_demo::DemoState st;
            s.build(ctx, st);
            ctx.Render();
        }
        glFinish();

        std::vector<unsigned char> px((size_t)width * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const int stride = width * 4;
        for (int y = 0; y < height; ++y)
            std::memcpy(&flipped[(size_t)y * stride], &px[(size_t)(height - 1 - y) * stride], stride);

        const std::string path = outDir + "/" + s.name + ".png";
        if (!stbi_write_png(path.c_str(), width, height, 4, flipped.data(), stride)) {
            std::fprintf(stderr, "failed to write %s\n", path.c_str());
            ++failures;
            continue;
        }
        std::printf("%s\t%s\n", s.name, path.c_str());
    }

    font.Unload();   // GL texture must be deleted before glfwTerminate drops the context
    glfwDestroyWindow(window);
    glfwTerminate();
    return failures ? 1 : 0;
}