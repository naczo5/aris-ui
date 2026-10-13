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
#include "scene_aurora.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

namespace {

struct Scene {
    const char* name;
    const char* description;
    // Builds the UI for one frame. `warmup`-many frames are rendered before
    // the snapshot so eased state (hover/press sliders) settles.
    void (*build)(ui::Context& ctx, aris_demo::DemoState& st);
    // Optional per-frame simulated input (mouse position / button state).
    void (*inject)(ui::Context& ctx) = nullptr;
    // Preferred framebuffer size (0 = use --width/--height).
    int width = 0, height = 0;
};

inline void Fill(ui::Context& ctx, const ui::Rect& r, ui::Color fill, float radius = 0.f, ui::Color border = ui::Color(0,0,0,0), float bw = 0.f) {
    ui::RectStyle s;
    s.fill = fill;
    s.cornerRadii = ui::Vec4(radius);
    s.borderColor = border;
    s.borderWidth = bw;
    ctx.DrawRect(r, s);
}

ui::Vec2 ScenePanelPos(const ui::Context&) { return ui::Vec2(28.f, 28.f); }

// --- scenes ----------------------------------------------------------------

void ScenePanel(ui::Context& ctx, aris_demo::DemoState& st) {
    aris_demo::BuildDemoUi(ctx, st);   // full canonical demo
}

// ---------------------------------------------------------------------------
// Scene: Neo-Brutalist Swiss Editorial (buttons / buttons-hover / buttons-press)
// Stark high-contrast architectural print design: solid canary yellow, cobalt,
// bold 2.5px jet-black borders, solid offset drop shadows, zero blur/gradients.
// ---------------------------------------------------------------------------
void DrawSwissButton(ui::Context& ctx, const char* id, const ui::Rect& r,
                     const char* label, ui::Icon icon, ui::Color fill, ui::Color textColor,
                     float shadowDist = 4.f, int forceState = 0) {
    ui::WidgetFrame w = ctx.Interact(id, r);
    float hov = w.state->hoverT;
    float prs = w.state->pressT;
    if (forceState == 1) hov = 1.f;
    if (forceState == 2) { prs = 1.f; hov = 1.f; }

    float offset = 0.f;
    float curShadow = shadowDist;
    ui::Color curFill = fill;
    ui::Color curText = textColor;

    if (prs > 0.1f) {
        // Pressed down flat into page: 0px shadow, inverted jet-black fill
        offset = shadowDist;
        curShadow = 0.f;
        curFill = ui::Color::Hex(0x111111);
        curText = fill; // High-contrast inverted fill
    } else if (hov > 0.1f) {
        // Hovered: button lifts up-left, shadow expands
        offset = -3.f * hov;
        curShadow = shadowDist + 3.f * hov;
        curFill = (fill == ui::Color::Hex(0xFFDE00)) ? ui::Color::Hex(0xFFEA38) : fill;
    }

    const ui::Rect rect = r.Offset(ui::Vec2(offset, offset));

    ui::RectStyle st;
    st.fill = curFill;
    st.cornerRadii = ui::Vec4(0.f); // sharp brutalist corners
    st.borderColor = ui::Color::Hex(0x111111);
    st.borderWidth = 2.5f;
    st.shadowColor = ui::Color::Hex(0x111111);
    st.shadowOffset = ui::Vec2(curShadow, curShadow);
    st.shadowBlur = 0.f; // crisp hard solid shadow
    st.shadowOpacity = 1.f;
    ctx.DrawRect(rect, st);

    const float cy = rect.Center().y;
    const float fs = 13.5f;
    const ui::Vec2 ts = ctx.MeasureText(label, fs);
    const float tw = ts.x + (icon != ui::Icon::Count ? 24.f : 0.f);
    float startX = rect.Center().x - tw * 0.5f;

    if (icon != ui::Icon::Count) {
        ui::DrawIcon(ctx, icon, ui::Rect(startX, cy - 8.f, 16.f, 16.f), curText, 2.0f);
        startX += 24.f;
    }
    ctx.DrawText(ui::Vec2(startX, cy - 8.f), label, curText, fs, nullptr, 0.45f);
}

void BuildSwissButtonsUi(ui::Context& ctx, int forceState = 0) {
    const float w = ctx.size().x, h = ctx.size().y;

    // Alabaster architectural paper background
    ui::RectStyle bg;
    bg.fill = ui::Color::Hex(0xF4F1EA);
    ctx.DrawRect(ui::Rect(0, 0, w, h), bg);

    // Subtle grid guide lines
    for (float x = 0; x < w; x += 30.f)
        ctx.DrawLine(ui::Vec2(x, 0), ui::Vec2(x, h), 1.f, ui::Color::Hex(0xE5E0D5));
    for (float y = 0; y < h; y += 30.f)
        ctx.DrawLine(ui::Vec2(0, y), ui::Vec2(w, y), 1.f, ui::Color::Hex(0xE5E0D5));

    // Editorial poster card
    const ui::Rect card(30.f, 20.f, w - 60.f, h - 40.f);
    ui::RectStyle cs;
    cs.fill = ui::Color::White();
    cs.cornerRadii = ui::Vec4(0.f);
    cs.borderColor = ui::Color::Hex(0x111111);
    cs.borderWidth = 2.5f;
    cs.shadowColor = ui::Color::Hex(0x111111);
    cs.shadowOffset = ui::Vec2(5.f, 5.f);
    cs.shadowBlur = 0.f;
    cs.shadowOpacity = 1.f;
    ctx.DrawRect(card, cs);

    // Header bar
    ctx.DrawText(ui::Vec2(card.x + 18.f, card.y + 16.f), "SWISS DESIGN // SYS-01",
                 ui::Color::Hex(0x111111), 14.f, nullptr, 0.5f);

    // Tag pill with generous padding
    const ui::Rect pill(card.Max().x - 134.f, card.y + 12.f, 116.f, 22.f);
    ui::RectStyle ps;
    ps.fill = ui::Color::Hex(0xF4F1EA);
    ps.borderColor = ui::Color::Hex(0x111111);
    ps.borderWidth = 1.5f;
    ctx.DrawRect(pill, ps);
    ctx.DrawText(ui::Vec2(pill.x + 10.f, pill.y + 5.f), "V-2.4 // STABLE", ui::Color::Hex(0x111111), 10.5f, nullptr, 0.35f);

    ctx.DrawLine(ui::Vec2(card.x, card.y + 42.f), ui::Vec2(card.Max().x, card.y + 42.f), 2.f, ui::Color::Hex(0x111111));

    // Primary action: Canary Yellow (receives hover / press state)
    DrawSwissButton(ctx, "btn_init", ui::Rect(card.x + 20.f, card.y + 56.f, card.w - 40.f, 46.f),
                    "INITIALIZE NEURAL INTERFACE", ui::Icon::Bolt,
                    ui::Color::Hex(0xFFDE00), ui::Color::Hex(0x111111), 4.f, forceState);

    // Dual secondary row: Cobalt Blue + Sharp Crimson
    const float halfW = (card.w - 52.f) * 0.5f;
    DrawSwissButton(ctx, "btn_combat", ui::Rect(card.x + 20.f, card.y + 116.f, halfW, 40.f),
                    "OVERCLOCK CORES", ui::Icon::Combat,
                    ui::Color::Hex(0x0047FF), ui::Color::White(), 3.5f, 0);
    DrawSwissButton(ctx, "btn_quantum", ui::Rect(card.x + 32.f + halfW, card.y + 116.f, halfW, 40.f),
                    "QUANTUM RADAR", ui::Icon::Visuals,
                    ui::Color::Hex(0xE11D48), ui::Color::White(), 3.5f, 0);

    // Tertiary row: Crisp White + Muted Slate
    DrawSwissButton(ctx, "btn_ghost1", ui::Rect(card.x + 20.f, card.y + 168.f, halfW, 38.f),
                    "DIAGNOSTICS", ui::Icon::Settings,
                    ui::Color::White(), ui::Color::Hex(0x111111), 3.f, 0);
    DrawSwissButton(ctx, "btn_ghost2", ui::Rect(card.x + 32.f + halfW, card.y + 168.f, halfW, 38.f),
                    "PURGE CACHE", ui::Icon::Close,
                    ui::Color::Hex(0xE2E8F0), ui::Color::Hex(0x111111), 3.f, 0);

    // Simulated mouse cursor for hover & press states
    if (forceState == 1) {
        const ui::Vec2 mp(card.x + 110.f, card.y + 80.f);
        ui::DrawIcon(ctx, ui::Icon::Cursor, ui::Rect(mp.x + 1.f, mp.y + 1.f, 18.f, 18.f), ui::Color(0, 0, 0, 180), 2.0f);
        ui::DrawIcon(ctx, ui::Icon::Cursor, ui::Rect(mp.x, mp.y, 18.f, 18.f), ui::Color::White(), 1.6f);
    } else if (forceState == 2) {
        const ui::Vec2 mp(card.x + 110.f, card.y + 82.f);
        ctx.DrawCircle(ui::Vec2(mp.x + 4.f, mp.y + 4.f), 10.f, ui::Color(255, 222, 0, 180));
        ui::DrawIcon(ctx, ui::Icon::Cursor, ui::Rect(mp.x + 1.f, mp.y + 1.f, 18.f, 18.f), ui::Color::Black(), 2.0f);
        ui::DrawIcon(ctx, ui::Icon::Cursor, ui::Rect(mp.x, mp.y, 18.f, 18.f), ui::Color::Hex(0xFFDE00), 1.6f);
    }

    // Monospaced telemetry chips with strong ink contrast
    const float fy = card.Max().y - 30.f;
    const char* chips[] = {"[BUFFER: 64MB]", "[LATENCY: 0.08ms]", "[TLS 1.3]", "[ACTIVE]"};
    float chipX = card.x + 20.f;
    for (const char* ch : chips) {
        const ui::Vec2 cs2 = ctx.MeasureText(ch, 11.5f);
        ctx.DrawText(ui::Vec2(chipX, fy + 2.f), ch, ui::Color::Hex(0x18181B), 11.5f, nullptr, 0.25f);
        chipX += cs2.x + 16.f;
    }
}

void SceneButtons(ui::Context& ctx, aris_demo::DemoState&) {
    BuildSwissButtonsUi(ctx, 0);
}

void SceneButtonsHover(ui::Context& ctx, aris_demo::DemoState&) {
    BuildSwissButtonsUi(ctx, 1);
}

void SceneButtonsPress(ui::Context& ctx, aris_demo::DemoState&) {
    BuildSwissButtonsUi(ctx, 2);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Scene: macOS Sequoia Frosted Glass (Toggles & Segmented Control)
// Understated Apple human interface design: translucent dark glass, segmented
// control, clean capsule toggles, soft diffuse ambient shadows.
// ---------------------------------------------------------------------------
void SceneToggles(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    ui::RectStyle bg;
    bg.fill = ui::Color::Hex(0x0C0E14);
    ctx.DrawRect(ui::Rect(0, 0, w, h), bg);

    const ui::Rect card(32.f, 20.f, w - 64.f, h - 40.f);
    ui::RectStyle cs;
    cs.fill = ui::Color::Hex(0x151922, 235);
    cs.cornerRadii = ui::Vec4(16.f);
    cs.borderColor = ui::Color(255, 255, 255, 25);
    cs.borderWidth = 1.f;
    cs.shadowColor = ui::Color::Black();
    cs.shadowBlur = 32.f;
    cs.shadowOpacity = 0.45f;
    ctx.DrawRect(card, cs);

    // Header
    ui::DrawIcon(ctx, ui::Icon::Settings, ui::Rect(card.x + 20.f, card.y + 18.f, 20.f, 20.f),
                 ui::Color::Hex(0x007AFF), 1.8f);
    ctx.DrawText(ui::Vec2(card.x + 48.f, card.y + 18.f), "Display & System Preferences",
                 ui::Color::White(), 15.f, nullptr, 0.45f);
    ctx.DrawText(ui::Vec2(card.x + 48.f, card.y + 36.f), "Configured for Apple Silicon & Metal pipeline",
                 ui::Color::Hex(0x94A3B8), 11.5f);

    // Segmented Control (Top)
    const float segY = card.y + 60.f, segW = card.w - 40.f, segH = 28.f;
    ui::Rect segR(card.x + 20.f, segY, segW, segH);
    ui::RectStyle segSt;
    segSt.fill = ui::Color::Hex(0x0A0D12, 220);
    segSt.cornerRadii = ui::Vec4(8.f);
    segSt.borderColor = ui::Color(255, 255, 255, 18);
    segSt.borderWidth = 1.f;
    ctx.DrawRect(segR, segSt);

    const float tabW = segW / 3.f;
    // Active tab indicator (Performance)
    ui::Rect activeTab(segR.x + 2.f, segR.y + 2.f, tabW - 4.f, segH - 4.f);
    ui::RectStyle atSt;
    atSt.fill = ui::Color::Hex(0x222B3A);
    atSt.cornerRadii = ui::Vec4(6.f);
    atSt.borderColor = ui::Color(255, 255, 255, 30);
    atSt.borderWidth = 1.f;
    ctx.DrawRect(activeTab, atSt);

    const char* tabs[] = {"Performance", "Balanced", "Low Power"};
    for (int t = 0; t < 3; ++t) {
        const ui::Vec2 ts = ctx.MeasureText(tabs[t], 11.5f);
        const float tx = segR.x + t * tabW + (tabW - ts.x) * 0.5f;
        ctx.DrawText(ui::Vec2(tx, segR.y + 7.f), tabs[t],
                     (t == 0) ? ui::Color::White() : ui::Color::Hex(0x94A3B8), 11.5f, nullptr, (t == 0) ? 0.35f : 0.f);
    }

    struct Row { const char* id; const char* title; const char* sub; bool on; ui::Color color; };
    static Row rows[] = {
        {"pm120", "ProMotion 120Hz Refresh", "Dynamic refresh rate scaling up to 120 FPS", true, ui::Color::Hex(0x007AFF)},
        {"p3gamut", "Wide Color Gamut (DCI-P3)", "10-bit deep color buffer with HDR tone mapping", true, ui::Color::Hex(0x34C759)},
        {"rtxmetal", "Hardware Ray Tracing", "Path-traced ambient occlusion and specular rays", true, ui::Color::Hex(0xAF52DE)},
        {"lowlat", "Low Latency Metal Reflex", "Direct queue dispatch bypassing host compositor", false, ui::Color::Hex(0x007AFF)},
    };

    float y = card.y + 102.f;
    for (int i = 0; i < 4; ++i) {
        ui::Rect rowRect(card.x + 16.f, y, card.w - 32.f, 52.f);
        ui::WidgetFrame wf = ctx.Interact(rows[i].id, rowRect);
        if (wf.clicked) rows[i].on = !rows[i].on;

        if (wf.state->hoverT > 0.01f) {
            ui::RectStyle hbg;
            hbg.cornerRadii = ui::Vec4(8.f);
            hbg.fill = ui::Color(255, 255, 255, (std::uint8_t)(10 * wf.state->hoverT));
            ctx.DrawRect(rowRect, hbg);
        }

        ctx.DrawText(ui::Vec2(rowRect.x + 12.f, rowRect.y + 10.f), rows[i].title,
                     ui::Color::Hex(0xF1F5F9), 13.5f, nullptr, 0.4f);
        ctx.DrawText(ui::Vec2(rowRect.x + 12.f, rowRect.y + 28.f), rows[i].sub,
                     ui::Color::Hex(0x94A3B8), 11.f);

        const float tw = 40.f, th = 22.f;
        ui::Rect track(rowRect.Max().x - tw - 12.f, rowRect.y + (rowRect.h - th) * 0.5f, tw, th);
        ui::RectStyle tr;
        tr.cornerRadii = ui::Vec4(th * 0.5f);
        tr.fill = rows[i].on ? rows[i].color : ui::Color::Hex(0x232C3A);
        tr.borderColor = ui::Color(255, 255, 255, rows[i].on ? 40 : 20);
        tr.borderWidth = 1.f;
        ctx.DrawRect(track, tr);

        const float knobSize = th - 4.f;
        const float knobX = rows[i].on ? track.Max().x - knobSize - 2.f : track.x + 2.f;
        ui::RectStyle ks;
        ks.fill = ui::Color::White();
        ks.cornerRadii = ui::Vec4(knobSize * 0.5f);
        ks.shadowOffset = ui::Vec2(0.f, 1.f);
        ks.shadowBlur = 3.f;
        ks.shadowOpacity = 0.35f;
        ctx.DrawRect(ui::Rect(knobX, track.y + 2.f, knobSize, knobSize), ks);

        y += 54.f;
    }
}

// ---------------------------------------------------------------------------
// Scene: Vintage Analog Audio Mastering Console (Sliders)
// Skeuomorphic dark steel chassis, screw rivets, calibrated dB tick marks,
// tactile aluminum fader caps, and discrete LED level meter strip.
// ---------------------------------------------------------------------------
void SceneSliders(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    ui::RectStyle bg;
    bg.fill = ui::Color::Hex(0x101317);
    ctx.DrawRect(ui::Rect(0, 0, w, h), bg);

    const ui::Rect card(32.f, 20.f, w - 64.f, h - 40.f);
    ui::RectStyle cs;
    cs.fill = ui::Color::Hex(0x1B1F26);
    cs.cornerRadii = ui::Vec4(6.f);
    cs.borderColor = ui::Color::Hex(0x323946);
    cs.borderWidth = 2.f;
    cs.shadowColor = ui::Color::Black();
    cs.shadowBlur = 30.f;
    cs.shadowOpacity = 0.5f;
    ctx.DrawRect(card, cs);

    // 4 Corner hex screw rivets
    auto drawScrew = [&](float sx, float sy) {
        ctx.DrawCircle(ui::Vec2(sx, sy), 5.f, ui::Color::Hex(0x323842));
        ctx.DrawCircle(ui::Vec2(sx, sy), 4.f, ui::Color::Hex(0x20242B));
        ctx.DrawLine(ui::Vec2(sx - 2.5f, sy), ui::Vec2(sx + 2.5f, sy), 1.f, ui::Color::Hex(0x4A5362));
    };
    drawScrew(card.x + 12.f, card.y + 12.f);
    drawScrew(card.Max().x - 12.f, card.y + 12.f);
    drawScrew(card.x + 12.f, card.Max().y - 12.f);
    drawScrew(card.Max().x - 12.f, card.Max().y - 12.f);

    // Stenciled console header
    ctx.DrawText(ui::Vec2(card.x + 28.f, card.y + 16.f), "ANALOG MASTERING CONSOLE // MODEL EQ-800",
                 ui::Color::Hex(0xE2E8F0), 13.5f, nullptr, 0.4f);
    ctx.DrawText(ui::Vec2(card.x + 28.f, card.y + 34.f), "Discrete passive inductor signal chain",
                 ui::Color::Hex(0x64748B), 11.f);

    // Solid amber power LED indicator
    ctx.DrawCircle(ui::Vec2(card.Max().x - 40.f, card.y + 24.f), 4.5f, ui::Color::Hex(0xF59E0B));
    ctx.DrawCircle(ui::Vec2(card.Max().x - 40.f, card.y + 24.f), 2.f, ui::Color::Hex(0xFEF08A));

    struct Chan {
        const char* label; const char* val; float frac;
    };
    static Chan chans[] = {
        {"SUB BASS [45Hz]", "+4.5 dB", 0.72f},
        {"VOCAL MID [2.4kHz]", "+1.5 dB", 0.56f},
        {"HIGH AIR [16kHz]", "+5.0 dB", 0.78f},
        {"OUTPUT TRIM", "0.0 dB", 0.50f},
    };

    float y = card.y + 60.f;
    for (int i = 0; i < 4; ++i) {
        ctx.DrawText(ui::Vec2(card.x + 28.f, y), chans[i].label, ui::Color::Hex(0xCBD5E1), 11.5f, nullptr, 0.35f);
        ctx.DrawText(ui::Vec2(card.Max().x - 85.f, y), chans[i].val, ui::Color::Hex(0xF59E0B), 11.5f, nullptr, 0.4f);

        const float rx = card.x + 28.f;
        const float rw = card.w - 56.f;
        const float ty = y + 26.f, th = 6.f;

        // Calibrated tick marks above the track (clear of fader knob top at ty - 7.f)
        for (int k = 0; k <= 8; ++k) {
            float tx = rx + (rw * k) / 8.f;
            ctx.DrawLine(ui::Vec2(tx, ty - 13.f), ui::Vec2(tx, ty - 8.f), 1.f, ui::Color::Hex(0x64748B));
        }

        // Recessed metal groove
        ui::Rect track(rx, ty, rw, th);
        ui::RectStyle tr;
        tr.cornerRadii = ui::Vec4(3.f);
        tr.fill = ui::Color::Hex(0x0C0E12);
        tr.borderColor = ui::Color::Hex(0x282E38);
        tr.borderWidth = 1.f;
        ctx.DrawRect(track, tr);

        // Tactile aluminum fader cap
        const float kx = rx + rw * chans[i].frac;
        ui::Rect knob(kx - 10.f, ty - 7.f, 20.f, 20.f);
        ui::RectStyle ks;
        ks.fill = ui::Color::Hex(0xD1D5DB);
        ks.cornerRadii = ui::Vec4(3.f);
        ks.borderColor = ui::Color::Hex(0x9CA3AF);
        ks.borderWidth = 1.f;
        ks.shadowColor = ui::Color::Black();
        ks.shadowOffset = ui::Vec2(0.f, 2.f);
        ks.shadowBlur = 4.f;
        ks.shadowOpacity = 0.5f;
        ctx.DrawRect(knob, ks);
        // Center grip line on fader
        ctx.DrawLine(ui::Vec2(knob.Center().x, knob.y + 3.f), ui::Vec2(knob.Center().x, knob.Max().y - 3.f),
                     1.5f, ui::Color::Hex(0x1F2937));

        y += 56.f;
    }

    // Discrete 14-dot LED meter strip across bottom
    const float my = card.Max().y - 28.f;
    ctx.DrawText(ui::Vec2(card.x + 28.f, my), "VU LEVEL", ui::Color::Hex(0x64748B), 10.f);
    float mx = card.x + 90.f;
    for (int d = 0; d < 14; ++d) {
        ui::Color ledCol = (d < 9) ? ui::Color::Hex(0x10B981) : (d < 12) ? ui::Color::Hex(0xF59E0B) : ui::Color::Hex(0xEF4444);
        if (d >= 11) ledCol = ledCol.WithAlpha(50); // inactive peak dots
        ctx.DrawCircle(ui::Vec2(mx + d * 18.f, my + 5.f), 3.5f, ledCol);
    }
}

// ---------------------------------------------------------------------------
// Scene: MSDF Typography & Font Engine Specimen (Text)
// ---------------------------------------------------------------------------
void SceneText(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;

    // Warm architectural paper / parchment
    ui::RectStyle bg;
    bg.fill = ui::Color::Hex(0xF6F3EC);
    ctx.DrawRect(ui::Rect(0, 0, w, h), bg);

    // Outer framing card with crisp black ink border
    const ui::Rect page(28.f, 20.f, w - 56.f, h - 40.f);
    ui::RectStyle ps;
    ps.fill = ui::Color::Hex(0xFAF8F2);
    ps.borderColor = ui::Color::Hex(0x18181B);
    ps.borderWidth = 2.f;
    ctx.DrawRect(page, ps);

    // Top masthead bar
    {
        ui::Rect tag(page.x + 20.f, page.y + 16.f, 160.f, 22.f);
        ui::RectStyle ts;
        ts.fill = ui::Color::Hex(0xD9381E);   // Bauhaus Vermillion Red
        ctx.DrawRect(tag, ts);
        ctx.DrawText(ui::Vec2(tag.x + 10.f, tag.y + 5.f), "TYPE SPECIMEN // v2.0",
                     ui::Color::White(), 11.f, nullptr, 0.45f);

        ctx.DrawText(ui::Vec2(page.Max().x - 220.f, page.y + 19.f),
                     "FREETYPE + MSDFGEN ENGINE", ui::Color::Hex(0x52525B), 11.f, nullptr, 0.3f);
    }

    // Heavy black rule
    ctx.DrawLine(ui::Vec2(page.x + 20.f, page.y + 46.f),
                 ui::Vec2(page.Max().x - 20.f, page.y + 46.f), 2.f, ui::Color::Hex(0x18181B));

    float y = page.y + 58.f;

    // Giant display specimen
    ctx.DrawText(ui::Vec2(page.x + 20.f, y), "Aa Bb Gg 78", ui::Color::Hex(0x09090B),
                 38.f, nullptr, 0.5f);
    const float gw = ctx.MeasureText("Aa Bb Gg 78", 38.f).x;
    ctx.DrawText(ui::Vec2(page.x + 36.f + gw, y + 8.f), "PRECISION VECTOR TYPE",
                 ui::Color::Hex(0xD9381E), 16.f, nullptr, 0.5f);
    y += 46.f;

    ctx.DrawText(ui::Vec2(page.x + 20.f, y),
                 "Multi-channel distance fields compute true Euclidean distances from glyph contours.",
                 ui::Color::Hex(0x3F3F46), 13.5f);
    y += 24.f;

    // Thin grid rule
    ctx.DrawLine(ui::Vec2(page.x + 20.f, y), ui::Vec2(page.Max().x - 20.f, y), 1.f,
                 ui::Color::Hex(0xE4E4E7));
    y += 14.f;

    // Typographic scale ladder with exact baseline alignment
    auto drawLadderRow = [&](const char* sizeLabel, const char* sample, float pxSize, float weight) {
        // Shared font baseline offset: (pxSize - 11.f) * ~0.8f
        const float labelY = y + (pxSize - 11.f) * 0.78f;
        ctx.DrawText(ui::Vec2(page.x + 20.f, labelY), sizeLabel, ui::Color::Hex(0xD9381E), 11.f, nullptr, 0.25f);
        ctx.DrawText(ui::Vec2(page.x + 72.f, y), sample, ui::Color::Hex(0x18181B), pxSize, nullptr, weight);
        y += pxSize + 10.f;
    };

    drawLadderRow("22 px", "Subpixel antialiasing with infinite zoom fidelity", 22.f, 0.35f);
    drawLadderRow("16 px", "Glyph metrics, baseline alignment & vector rendering", 16.f, 0.20f);
    drawLadderRow("13 px", "The quick brown fox jumps over the lazy dog. 0123456789", 13.f, 0.10f);
    drawLadderRow("11 px", "CHARSET: ASCII 32-126 + SYMBOLS // RUNTIME ATLAS 1024x1024", 11.f, 0.25f);

    y += 10.f;

    // Technical metrics banner (filling page void)
    {
        ui::Rect metBox(page.x + 20.f, y, page.w - 40.f, 22.f);
        Fill(ctx, metBox, ui::Color::Hex(0xF0EDE4), 0.f, ui::Color::Hex(0x18181B), 1.f);
        ctx.DrawText(ui::Vec2(metBox.x + 10.f, metBox.y + 4.5f),
                     "ASCENDER: +0.82 em  |  DESCENDER: -0.22 em  |  MSDF RANGE: 4.0 px  |  PASS: 1-DRAW CALL",
                     ui::Color::Hex(0x52525B), 10.5f, nullptr, 0.3f);
        y += 30.f;
    }

    // Bottom code callout box
    {
        const ui::Rect codeBox(page.x + 20.f, y, page.w - 40.f, 44.f);
        ui::RectStyle cbs;
        cbs.fill = ui::Color::Hex(0xEDE9DF);
        cbs.borderColor = ui::Color::Hex(0x18181B);
        cbs.borderWidth = 1.5f;
        ctx.DrawRect(codeBox, cbs);

        // Vermillion indicator tab
        ui::RectStyle vTab;
        vTab.fill = ui::Color::Hex(0xD9381E);
        ctx.DrawRect(ui::Rect(codeBox.x, codeBox.y, 5.f, codeBox.h), vTab);

        ctx.DrawText(ui::Vec2(codeBox.x + 16.f, codeBox.y + 14.f),
                     "ctx.DrawText(pos, \"Crisp Vector Text\", Color::Black(), 16.f, nullptr, 0.35f);",
                     ui::Color::Hex(0x18181B), 12.f);

        ctx.DrawText(ui::Vec2(codeBox.Max().x - 110.f, codeBox.y + 15.f),
                     "[OPENGL 3.3+]", ui::Color::Hex(0x71717A), 10.5f, nullptr, 0.3f);
    }
}


// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Scene: Cyber-Tactical Sci-Fi HUD Deck (Card)
// Wireframe military blueprint HUD: tactical corner brackets, telemetry readouts,
// wireframe targeting viewports, and segmented tick bars.
// ---------------------------------------------------------------------------
void SceneCard(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    ui::RectStyle bg;
    bg.fill = ui::Color::Hex(0x06080E);
    ctx.DrawRect(ui::Rect(0, 0, w, h), bg);

    // Subtle tactical dot grid
    for (float gx = 20.f; gx < w; gx += 36.f) {
        for (float gy = 20.f; gy < h; gy += 36.f) {
            ctx.DrawCircle(ui::Vec2(gx, gy), 1.f, ui::Color(56, 189, 248, 25));
        }
    }

    struct TacticalUnit {
        const char* callsign; const char* role; const char* freq;
        float powerFrac; float signalFrac;
        ui::Color accent; ui::Icon icon;
        const char* metric1; const char* val1;
        const char* metric2; const char* val2;
    };
    const TacticalUnit units[] = {
        {"RECON // MK-IV", "Orbital Drone", "FREQ: 868.4 MHz", 0.85f, 0.92f, ui::Color::Hex(0x00F2FE), ui::Icon::Bolt,
         "BANDWIDTH", "1.4 GB/s", "LATENCY", "0.04 ms"},
        {"AEGIS // S-01", "Kinetic Barrier", "CAP: 14.2 kJ", 0.68f, 0.88f, ui::Color::Hex(0xF59E0B), ui::Icon::Shield,
         "ABSORPTION", "94.2%", "RECHARGE", "1.2 s"},
        {"SPECTER // V2", "Stealth Sensor", "PULSE: 0.12 ms", 0.94f, 0.76f, ui::Color::Hex(0x10B981), ui::Icon::Visuals,
         "COVERAGE", "360 DEG", "ENCRYPT", "AES-256"},
    };

    const float cw = (w - 72.f - 36.f) / 3.f;
    const float ch = h - 56.f;
    float cx = 36.f;

    for (int i = 0; i < 3; ++i) {
        const ui::Rect cr(cx, 28.f, cw, ch);

        // Dark tech card background
        ui::RectStyle cs;
        cs.fill = ui::Color::Hex(0x0C1019, 240);
        cs.cornerRadii = ui::Vec4(0.f); // sharp tactical edges
        cs.borderColor = ui::Color(255, 255, 255, 20);
        cs.borderWidth = 1.f;
        ctx.DrawRect(cr, cs);

        // Tactical 4-Corner L-Brackets
        const float bLen = 14.f;
        const ui::Color bCol = units[i].accent;
        // Top-Left
        ctx.DrawLine(ui::Vec2(cr.x, cr.y), ui::Vec2(cr.x + bLen, cr.y), 2.f, bCol);
        ctx.DrawLine(ui::Vec2(cr.x, cr.y), ui::Vec2(cr.x, cr.y + bLen), 2.f, bCol);
        // Top-Right
        ctx.DrawLine(ui::Vec2(cr.Max().x - bLen, cr.y), ui::Vec2(cr.Max().x, cr.y), 2.f, bCol);
        ctx.DrawLine(ui::Vec2(cr.Max().x, cr.y), ui::Vec2(cr.Max().x, cr.y + bLen), 2.f, bCol);
        // Bottom-Left
        ctx.DrawLine(ui::Vec2(cr.x, cr.Max().y), ui::Vec2(cr.x + bLen, cr.Max().y), 2.f, bCol);
        ctx.DrawLine(ui::Vec2(cr.x, cr.Max().y - bLen), ui::Vec2(cr.x, cr.Max().y), 2.f, bCol);
        // Bottom-Right
        ctx.DrawLine(ui::Vec2(cr.Max().x - bLen, cr.Max().y), ui::Vec2(cr.Max().x, cr.Max().y), 2.f, bCol);
        ctx.DrawLine(ui::Vec2(cr.Max().x, cr.Max().y - bLen), ui::Vec2(cr.Max().x, cr.Max().y), 2.f, bCol);

        // Header callsign & status badge
        ctx.DrawText(ui::Vec2(cr.x + 14.f, cr.y + 14.f), units[i].callsign,
                     ui::Color::White(), 13.f, nullptr, 0.45f);
        ctx.DrawText(ui::Vec2(cr.x + 14.f, cr.y + 32.f), units[i].role,
                     units[i].accent, 11.f, nullptr, 0.2f);

        // Status pill badge (guaranteed no overlap)
        const ui::Rect sysPill(cr.Max().x - 66.f, cr.y + 14.f, 52.f, 18.f);
        Fill(ctx, sysPill, ui::Color::Hex(0x064E3B, 220), 3.f, ui::Color::Hex(0x10B981), 1.f);
        ctx.DrawText(ui::Vec2(sysPill.x + 6.f, sysPill.y + 3.5f), "ONLINE", ui::Color::Hex(0x6EE7B7), 9.5f, nullptr, 0.3f);

        // Targeting wireframe box
        const ui::Rect targetBox(cr.x + 14.f, cr.y + 52.f, cr.w - 28.f, 96.f);
        ui::RectStyle tbs;
        tbs.fill = ui::Color::Hex(0x05070B);
        tbs.borderColor = ui::Color(255, 255, 255, 22);
        tbs.borderWidth = 1.f;
        ctx.DrawRect(targetBox, tbs);

        // Reticle crosshair inside target box with high contrast
        const ui::Vec2 tc = targetBox.Center();
        ctx.DrawCircle(tc, 28.f, ui::Color(units[i].accent.r, units[i].accent.g, units[i].accent.b, 140), 1.2f);
        ctx.DrawCircle(tc, 16.f, ui::Color(units[i].accent.r, units[i].accent.g, units[i].accent.b, 200), 1.2f);
        ctx.DrawCircle(tc, 3.5f, units[i].accent);
        ctx.DrawLine(ui::Vec2(tc.x - 34.f, tc.y), ui::Vec2(tc.x + 34.f, tc.y), 1.2f,
                     ui::Color(units[i].accent.r, units[i].accent.g, units[i].accent.b, 160));
        ctx.DrawLine(ui::Vec2(tc.x, tc.y - 34.f), ui::Vec2(tc.x, tc.y + 34.f), 1.2f,
                     ui::Color(units[i].accent.r, units[i].accent.g, units[i].accent.b, 160));
        ui::DrawIcon(ctx, units[i].icon, ui::Rect(tc.x - 10.f, tc.y - 10.f, 20.f, 20.f), units[i].accent, 2.0f);

        // Frequency readout
        ctx.DrawText(ui::Vec2(cr.x + 14.f, cr.y + 158.f), units[i].freq, ui::Color::Hex(0x94A3B8), 10.5f);

        // Segmented power meter
        ctx.DrawText(ui::Vec2(cr.x + 14.f, cr.y + 178.f), "POWER OUTPUT", ui::Color::Hex(0xE2E8F0), 10.5f, nullptr, 0.2f);
        const float segY = cr.y + 194.f;
        const int totalTicks = 12;
        const float tickGap = 3.f;
        const float tickW = (cr.w - 28.f - (totalTicks - 1) * tickGap) / totalTicks;
        const int litTicks = (int)(totalTicks * units[i].powerFrac);
        for (int t = 0; t < totalTicks; ++t) {
            ui::Rect tr(cr.x + 14.f + t * (tickW + tickGap), segY, tickW, 7.f);
            ui::RectStyle ts;
            ts.fill = (t < litTicks) ? units[i].accent : ui::Color::Hex(0x161C26);
            ctx.DrawRect(tr, ts);
        }

        // Segmented signal meter
        ctx.DrawText(ui::Vec2(cr.x + 14.f, cr.y + 210.f), "SIGNAL FIDELITY", ui::Color::Hex(0xE2E8F0), 10.5f, nullptr, 0.2f);
        const float segY2 = cr.y + 226.f;
        const int litTicks2 = (int)(totalTicks * units[i].signalFrac);
        for (int t = 0; t < totalTicks; ++t) {
            ui::Rect tr(cr.x + 14.f + t * (tickW + tickGap), segY2, tickW, 7.f);
            ui::RectStyle ts;
            ts.fill = (t < litTicks2) ? ui::Color::Hex(0x38BDF8) : ui::Color::Hex(0x161C26);
            ctx.DrawRect(tr, ts);
        }

        // Telemetry stats grid (fills void between meters and button)
        ctx.DrawLine(ui::Vec2(cr.x + 14.f, cr.y + 246.f), ui::Vec2(cr.Max().x - 14.f, cr.y + 246.f),
                     1.f, ui::Color(255, 255, 255, 18));

        const float ty1 = cr.y + 256.f, ty2 = cr.y + 284.f;
        // Col 1
        ctx.DrawText(ui::Vec2(cr.x + 14.f, ty1), units[i].metric1, ui::Color::Hex(0x64748B), 9.5f);
        ctx.DrawText(ui::Vec2(cr.x + 14.f, ty1 + 12.f), units[i].val1, ui::Color::White(), 11.f, nullptr, 0.3f);
        // Col 2
        ctx.DrawText(ui::Vec2(cr.Center().x + 6.f, ty1), units[i].metric2, ui::Color::Hex(0x64748B), 9.5f);
        ctx.DrawText(ui::Vec2(cr.Center().x + 6.f, ty1 + 12.f), units[i].val2, units[i].accent, 11.f, nullptr, 0.3f);

        // Action deploy button (perfectly vertically centered text)
        const ui::Rect btnR(cr.x + 14.f, cr.Max().y - 38.f, cr.w - 28.f, 28.f);
        ui::RectStyle bs;
        bs.fill = ui::Color::Hex(0x06090F);
        bs.borderColor = units[i].accent;
        bs.borderWidth = 1.2f;
        ctx.DrawRect(btnR, bs);
        const ui::Vec2 bts = ctx.MeasureText("DEPLOY UNIT [F]", 11.f);
        ctx.DrawText(ui::Vec2(btnR.Center().x - bts.x * 0.5f, btnR.Center().y - bts.y * 0.5f),
                     "DEPLOY UNIT [F]", units[i].accent, 11.f, nullptr, 0.35f);

        cx += cw + 18.f;
    }
}

// ---------------------------------------------------------------------------
// Scene: Animation & Physics Oscilloscope (Time-Driven Motion)
// Second-order spring dynamics, continuous radar pulse rings, and phase-shifted
// dynamic curves compatible with real-time playback and video export.
// ---------------------------------------------------------------------------
void SceneAnimation(ui::Context& ctx, aris_demo::DemoState& st) {
    const float w = ctx.size().x, h = ctx.size().y;
    ui::RectStyle bg;
    bg.fill = ui::Color::Hex(0x080C14);
    ctx.DrawRect(ui::Rect(0, 0, w, h), bg);

    const ui::Rect card(32.f, 20.f, w - 64.f, h - 40.f);
    ui::RectStyle cs;
    cs.fill = ui::Color::Hex(0x0F1522, 245);
    cs.cornerRadii = ui::Vec4(14.f);
    cs.borderColor = ui::Color(0, 242, 254, 45);
    cs.borderWidth = 1.f;
    cs.shadowColor = ui::Color::Black();
    cs.shadowBlur = 32.f;
    cs.shadowOpacity = 0.4f;
    ctx.DrawRect(card, cs);

    // Header
    ui::DrawIcon(ctx, ui::Icon::Bolt, ui::Rect(card.x + 20.f, card.y + 16.f, 20.f, 20.f),
                 ui::Color::Hex(0x00F2FE), 1.8f);
    ctx.DrawText(ui::Vec2(card.x + 48.f, card.y + 16.f), "REALTIME DYNAMICS & PHYSICS OSCILLOSCOPE",
                 ui::Color::White(), 15.f, nullptr, 0.45f);
    ctx.DrawText(ui::Vec2(card.x + 48.f, card.y + 34.f),
                 "Second-order springs, continuous oscillators & multi-curve interpolation",
                 ui::Color::Hex(0x94A3B8), 11.5f);

    const float gw = (card.w - 56.f) * 0.58f;
    const float gh = 175.f;
    const ui::Rect graphBox(card.x + 20.f, card.y + 60.f, gw, gh);
    ui::RectStyle gbs;
    gbs.cornerRadii = ui::Vec4(8.f);
    gbs.fill = ui::Color::Hex(0x060910);
    gbs.borderColor = ui::Color(255, 255, 255, 20);
    gbs.borderWidth = 1.f;
    ctx.DrawRect(graphBox, gbs);

    // Reference grid
    ctx.DrawLine(ui::Vec2(graphBox.x + 10.f, graphBox.y + gh * 0.25f),
                 ui::Vec2(graphBox.Max().x - 10.f, graphBox.y + gh * 0.25f), 1.f, ui::Color(255, 255, 255, 12));
    ctx.DrawLine(ui::Vec2(graphBox.x + 10.f, graphBox.y + gh * 0.50f),
                 ui::Vec2(graphBox.Max().x - 10.f, graphBox.y + gh * 0.50f), 1.f, ui::Color(255, 255, 255, 12));
    ctx.DrawLine(ui::Vec2(graphBox.x + 10.f, graphBox.y + gh * 0.75f),
                 ui::Vec2(graphBox.Max().x - 10.f, graphBox.y + gh * 0.75f), 1.f, ui::Color(255, 255, 255, 12));

    // Header row inside graph box for Legend (separate from waveform plotting)
    struct Legend { const char* label; ui::Color col; };
    const Legend leg[] = {
        {"EaseOutCubic",   ui::Color::Hex(0x00F2FE)},
        {"EaseOutBack",    ui::Color::Hex(0xF59E0B)},
        {"EaseOutElastic", ui::Color::Hex(0xEC4899)},
        {"EaseOutBounce",  ui::Color::Hex(0x10B981)},
    };
    const float legItemW = (gw - 28.f) / 4.f;
    for (int i = 0; i < 4; ++i) {
        const float lx = graphBox.x + 14.f + i * legItemW;
        const float ly = graphBox.y + 8.f;
        ctx.DrawCircle(ui::Vec2(lx + 4.f, ly + 6.f), 3.f, leg[i].col);
        ctx.DrawText(ui::Vec2(lx + 11.f, ly + 1.f), leg[i].label, ui::Color::Hex(0xCBD5E1), 9.5f, nullptr, 0.2f);
    }

    // Grid line below legend header
    ctx.DrawLine(ui::Vec2(graphBox.x + 10.f, graphBox.y + 24.f),
                 ui::Vec2(graphBox.Max().x - 10.f, graphBox.y + 24.f), 1.f, ui::Color(255, 255, 255, 18));

    // Reference grid lines
    ctx.DrawLine(ui::Vec2(graphBox.x + 10.f, graphBox.y + 60.f),
                 ui::Vec2(graphBox.Max().x - 10.f, graphBox.y + 60.f), 1.f, ui::Color(255, 255, 255, 10));
    ctx.DrawLine(ui::Vec2(graphBox.x + 10.f, graphBox.y + 105.f),
                 ui::Vec2(graphBox.Max().x - 10.f, graphBox.y + 105.f), 1.f, ui::Color(255, 255, 255, 10));
    ctx.DrawLine(ui::Vec2(graphBox.x + 10.f, graphBox.y + 150.f),
                 ui::Vec2(graphBox.Max().x - 10.f, graphBox.y + 150.f), 1.f, ui::Color(255, 255, 255, 10));

    // Dynamic wave phase shift from st.time
    const float tPhase = st.time * 0.35f;
    auto plotCurve = [&](auto fn, ui::Color col, float yBase, float scaleY) {
        ui::Vec2 prev(0, 0);
        float prevEvalT = -1.f;
        for (int i = 0; i <= 90; ++i) {
            const float t = (float)i / 90.f;
            const float evalT = std::fmod(t + tPhase, 1.0f);
            const float val = fn(evalT);
            const float px = graphBox.x + 14.f + t * (gw - 28.f);
            const float py = yBase - val * scaleY;
            // Prevent drawing a connecting vertical tear across wrap boundary
            if (i > 0 && evalT >= prevEvalT) {
                ctx.DrawLine(prev, ui::Vec2(px, py), 2.f, col);
            }
            prev = ui::Vec2(px, py);
            prevEvalT = evalT;
        }
    };

    const float plotBase = graphBox.Max().y - 14.f;
    const float plotScale = gh - 78.f;
    plotCurve(ui::EaseOutCubic, ui::Color::Hex(0x00F2FE), plotBase, plotScale);
    plotCurve(ui::EaseOutBack, ui::Color::Hex(0xF59E0B), plotBase, plotScale);
    plotCurve(ui::EaseOutElastic, ui::Color::Hex(0xEC4899), plotBase, plotScale);
    plotCurve(ui::EaseOutBounce, ui::Color::Hex(0x10B981), plotBase, plotScale);

    // Continuous pulsating radar on the right
    const float rw = card.w - gw - 56.f;
    const ui::Rect rightBox(card.x + gw + 36.f, card.y + 60.f, rw, gh);
    ui::RectStyle rbs;
    rbs.cornerRadii = ui::Vec4(8.f);
    rbs.fill = ui::Color::Hex(0x060910);
    rbs.borderColor = ui::Color(255, 255, 255, 20);
    rbs.borderWidth = 1.f;
    ctx.DrawRect(rightBox, rbs);

    const ui::Vec2 rc = rightBox.Center() - ui::Vec2(0.f, 10.f);
    ctx.DrawCircle(rc, 55.f, ui::Color(0, 242, 254, 30), 1.f);
    ctx.DrawCircle(rc, 36.f, ui::Color(0, 242, 254, 55), 1.f);
    ctx.DrawCircle(rc, 18.f, ui::Color(0, 242, 254, 85), 1.f);
    ctx.DrawCircle(rc, 4.f, ui::Color::Hex(0x00F2FE));

    // Dynamic expanding pulse wave based on st.time
    for (int p = 0; p < 2; ++p) {
        float rPulse = std::fmod(st.time * 28.f + p * 27.f, 55.f);
        float pulseAlpha = std::max(0.f, 1.f - rPulse / 55.f);
        ctx.DrawCircle(rc, rPulse, ui::Color(0, 242, 254, (std::uint8_t)(150 * pulseAlpha)), 1.5f);
    }

    ctx.DrawLine(ui::Vec2(rc.x - 60.f, rc.y), ui::Vec2(rc.x + 60.f, rc.y), 1.f, ui::Color(0, 242, 254, 45));
    ctx.DrawLine(ui::Vec2(rc.x, rc.y - 60.f), ui::Vec2(rc.x, rc.y + 60.f), 1.f, ui::Color(0, 242, 254, 45));
    ctx.DrawText(ui::Vec2(rightBox.x + 12.f, rightBox.Max().y - 20.f), "PULSE RADAR OSCILLATOR",
                 ui::Color::Hex(0x00F2FE), 10.5f, nullptr, 0.4f);

    // Spring damping bars across bottom
    const float by = card.y + 250.f;
    const float bw = card.w - 40.f;
    ui::Rect bottomBox(card.x + 20.f, by, bw, card.h - 266.f);
    ui::RectStyle bbs;
    bbs.cornerRadii = ui::Vec4(8.f);
    bbs.fill = ui::Color::Hex(0x060910);
    bbs.borderColor = ui::Color(255, 255, 255, 20);
    bbs.borderWidth = 1.f;
    ctx.DrawRect(bottomBox, bbs);

    ctx.DrawText(ui::Vec2(bottomBox.x + 16.f, bottomBox.y + 12.f),
                 "SPRING HARMONIC OSCILLATORS",
                 ui::Color::Hex(0xE2E8F0), 11.5f, nullptr, 0.4f);

    ctx.DrawText(ui::Vec2(bottomBox.Max().x - 200.f, bottomBox.y + 12.f),
                 "[zeta = 0.62  |  w0 = 18 rad/s]", ui::Color::Hex(0x64748B), 10.5f);

    auto drawSpringBar = [&](float yPos, const char* label, float frac, ui::Color c1, ui::Color c2) {
        ctx.DrawText(ui::Vec2(bottomBox.x + 16.f, yPos), label, ui::Color::Hex(0xCBD5E1), 11.5f);
        ui::Rect tr(bottomBox.x + 210.f, yPos + 2.f, bottomBox.w - 230.f, 10.f);
        ui::RectStyle trSt;
        trSt.cornerRadii = ui::Vec4(5.f);
        trSt.fill = ui::Color::Hex(0x161C26);
        ctx.DrawRect(tr, trSt);

        ui::RectStyle fSt;
        fSt.cornerRadii = ui::Vec4(5.f);
        fSt.fill = c1;
        fSt.fillEnd = c2;
        fSt.gradientDir = ui::Vec2(1.f, 0.f);
        ctx.DrawRect(ui::Rect(tr.x, tr.y, tr.w * ui::Clamp01(frac), 10.f), fSt);
    };

    // 3 animated spring tracks (filling the bottom box completely)
    const float frac1 = 0.5f + 0.38f * std::sin(st.time * 2.8f);
    const float frac2 = 0.5f + 0.30f * std::cos(st.time * 2.0f);
    const float frac3 = 0.5f + 0.42f * std::sin(st.time * 3.6f + 1.2f);
    drawSpringBar(bottomBox.y + 34.f, "Spring: Bouncy (220, 12)", frac1,
                  ui::Color::Hex(0x00F2FE), ui::Color::Hex(0x3B82F6));
    drawSpringBar(bottomBox.y + 54.f, "Spring: Smooth (160, 20)", frac2,
                  ui::Color::Hex(0x10B981), ui::Color::Hex(0x84CC16));
    drawSpringBar(bottomBox.y + 74.f, "Spring: Snappy (280, 16)", frac3,
                  ui::Color::Hex(0xF59E0B), ui::Color::Hex(0xEF4444));
}

// ---------------------------------------------------------------------------
// // ---------------------------------------------------------------------------
// Competitive Game Overlays (Standalone competitive HUD elements on #080C14)
// High-fidelity arraylist, floating nametags, 2D corner-box ESP with skeleton,
// and unified client HUD. Absolute coordinate rendering with zero backdrop.
// ---------------------------------------------------------------------------

// Top-left client watermark pill
void DrawWatermark(ui::Context& ctx, float x, float y) {
    const ui::Rect pill(x, y, 235.f, 30.f);
    ui::RectStyle ps;
    ps.fill = ui::Color::Hex(0x0C131F, 240);
    ps.cornerRadii = ui::Vec4(6.f);
    ps.borderColor = ui::Color(56, 189, 248, 60);
    ps.borderWidth = 1.f;
    ps.shadowColor = ui::Color::Black();
    ps.shadowBlur = 12.f;
    ps.shadowOpacity = 0.4f;
    ctx.DrawRect(pill, ps);

    // Left accent bar
    Fill(ctx, ui::Rect(pill.x, pill.y + 4.f, 3.f, pill.h - 8.f), ui::Color::Hex(0x00F2FE), 1.5f);

    ctx.DrawText(ui::Vec2(pill.x + 12.f, pill.y + 7.f), "aris-ui", ui::Color::White(), 14.f, nullptr, 0.4f);
    const float tw = ctx.MeasureText("aris-ui", 14.f).x;
    ctx.DrawText(ui::Vec2(pill.x + 12.f + tw + 6.f, pill.y + 8.5f), "v0.1", ui::Color::Hex(0x64748B), 11.5f);
    ctx.DrawText(ui::Vec2(pill.x + 12.f + tw + 34.f, pill.y + 8.f), "|  240 FPS  |  12ms", ui::Color::Hex(0x38BDF8), 11.5f);
}

// Top-right module arraylist: strictly sorted descending by rendered pixel width
void DrawArraylist(ui::Context& ctx, float rightX, float topY, int limit = 10) {
    struct Mod { const char* name; const char* tag; ui::Color col; };
    const Mod rawMods[] = {
        {"Velocity",       " [Standard - 90%]", ui::Color::Hex(0x00F2FE)},
        {"InventoryMove",  " [Sneak Mode]",     ui::Color::Hex(0x38BDF8)},
        {"KillAura",       " [Single - 3.8m]",  ui::Color::Hex(0x60A5FA)},
        {"AutoArmor",      " [Smart P4]",       ui::Color::Hex(0x818CF8)},
        {"ChestStealer",   " [Silent]",         ui::Color::Hex(0xA78BFA)},
        {"Speed",          " [Bhop - 0.42]",    ui::Color::Hex(0xC084FC)},
        {"FastPlace",      " [Delay: 0ms]",     ui::Color::Hex(0xE879F9)},
        {"Flight",         " [Hypixel Bypass]", ui::Color::Hex(0xF43F5E)},
        {"NoFall",         " [Packet Mode]",    ui::Color::Hex(0xFB923C)},
        {"Sprint",         " [Omni-Direction]", ui::Color::Hex(0x34D399)},
    };

    const float fs = 13.f;
    const float rowH = 22.f;

    struct EvalMod {
        const char* name;
        const char* tag;
        ui::Color col;
        float nw;
        float gw;
        float totalW;
    };
    std::vector<EvalMod> sorted;
    for (const auto& m : rawMods) {
        float nw = ctx.MeasureText(m.name, fs).x;
        float gw = ctx.MeasureText(m.tag, fs - 1.f).x;
        sorted.push_back({m.name, m.tag, m.col, nw, gw, nw + gw + 18.f});
    }
    // Mathematically guaranteed strictly descending cascade
    std::sort(sorted.begin(), sorted.end(), [](const EvalMod& a, const EvalMod& b) {
        return a.totalW > b.totalW;
    });

    float y = topY;
    const int count = std::min(limit, (int)sorted.size());
    for (int i = 0; i < count; ++i) {
        const auto& m = sorted[i];
        const ui::Rect row(rightX - m.totalW, y, m.totalW, rowH);

        // Row background pill
        ui::RectStyle rs;
        rs.fill = ui::Color::Hex(0x0B1220, 235);
        rs.cornerRadii = ui::Vec4(4.f, 0.f, 0.f, 4.f);
        rs.borderColor = ui::Color(255, 255, 255, 12);
        rs.borderWidth = 0.8f;
        ctx.DrawRect(row, rs);

        // Right-edge chromatic accent bar
        Fill(ctx, ui::Rect(rightX - 2.5f, y, 2.5f, rowH), m.col, 0.f);

        // Module name and setting tag
        ctx.DrawText(ui::Vec2(row.x + 8.f, y + 4.f), m.name, ui::Color::Hex(0xF8FAFC), fs, nullptr, 0.2f);
        ctx.DrawText(ui::Vec2(row.x + 8.f + m.nw, y + 4.5f), m.tag, ui::Color::Hex(0x94A3B8), fs - 1.f);

        y += rowH + 2.f;
    }
}

void SceneArraylist(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    Fill(ctx, ui::Rect(0, 0, w, h), ui::Color::Hex(0x080C14));

    // 1. Watermark (Top-Left)
    DrawWatermark(ctx, 24.f, 20.f);

    // 2. Cascading Arraylist (Top-Right)
    DrawArraylist(ctx, w - 24.f, 20.f, 10);

    // 3. Active Keybinds Card (Left)
    {
        const ui::Rect kbCard(24.f, 68.f, 175.f, 140.f);
        ui::RectStyle ks;
        ks.fill = ui::Color::Hex(0x0B1220, 230);
        ks.cornerRadii = ui::Vec4(8.f);
        ks.borderColor = ui::Color(255, 255, 255, 18);
        ks.borderWidth = 1.f;
        ctx.DrawRect(kbCard, ks);

        ctx.DrawText(ui::Vec2(kbCard.x + 12.f, kbCard.y + 10.f), "KEYBINDS",
                     ui::Color::Hex(0x64748B), 10.f, nullptr, 0.35f);

        auto drawBind = [&](float py, const char* key, const char* mod, ui::Color col) {
            ctx.DrawText(ui::Vec2(kbCard.x + 12.f, py), key, col, 11.5f, nullptr, 0.3f);
            ctx.DrawText(ui::Vec2(kbCard.x + 36.f, py), mod, ui::Color::Hex(0xE2E8F0), 11.5f);
        };
        drawBind(kbCard.y + 30.f, "[R]", "KillAura", ui::Color::Hex(0x00F2FE));
        drawBind(kbCard.y + 52.f, "[V]", "Velocity", ui::Color::Hex(0x38BDF8));
        drawBind(kbCard.y + 74.f, "[F]", "Speed", ui::Color::Hex(0xC084FC));
        drawBind(kbCard.y + 96.f, "[G]", "Flight", ui::Color::Hex(0x94A3B8));
        drawBind(kbCard.y + 118.f, "[B]", "AutoArmor", ui::Color::Hex(0x94A3B8));
    }

    // 4. Session Stats Card (Bottom-Left)
    {
        const ui::Rect stCard(24.f, h - 84.f, 210.f, 64.f);
        ui::RectStyle ss;
        ss.fill = ui::Color::Hex(0x0B1220, 230);
        ss.cornerRadii = ui::Vec4(8.f);
        ss.borderColor = ui::Color(255, 255, 255, 18);
        ss.borderWidth = 1.f;
        ctx.DrawRect(stCard, ss);

        ctx.DrawText(ui::Vec2(stCard.x + 12.f, stCard.y + 10.f), "SESSION SUMMARY",
                     ui::Color::Hex(0x64748B), 10.f, nullptr, 0.35f);
        ctx.DrawText(ui::Vec2(stCard.x + 12.f, stCard.y + 26.f), "Kills: 14  |  Deaths: 0  |  K/D: 14.0",
                     ui::Color::Hex(0xE2E8F0), 11.f);
        ctx.DrawText(ui::Vec2(stCard.x + 12.f, stCard.y + 44.f), "Win Streak: 14  |  Time: 22m",
                     ui::Color::Hex(0x94A3B8), 10.5f);
    }

    // 5. In-game Coordinates HUD (Bottom-Right)
    ctx.DrawText(ui::Vec2(w - 230.f, h - 26.f), "XYZ: 142.5 / 64.0 / -820.3 (North)",
                 ui::Color::Hex(0x64748B), 10.5f);
}

void SceneNametags(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    Fill(ctx, ui::Rect(0, 0, w, h), ui::Color::Hex(0x080C14));

    // Subtle perspective grid ground circles
    ctx.DrawCircle(ui::Vec2(w * 0.5f, h * 0.88f), 200.f, ui::Color(255, 255, 255, 6), 1.f);
    ctx.DrawCircle(ui::Vec2(w * 0.5f, h * 0.88f), 120.f, ui::Color(255, 255, 255, 8), 1.f);

    auto drawPlayerSilhouette = [&](float cx, float feetY, float scale, ui::Color aura) {
        // Ground shadow
        Fill(ctx, ui::Rect(cx - 24.f * scale, feetY - 4.f * scale, 48.f * scale, 8.f * scale),
             ui::Color(0, 0, 0, 180), 4.f * scale);
        // Legs
        Fill(ctx, ui::Rect(cx - 10.f * scale, feetY - 30.f * scale, 8.f * scale, 28.f * scale),
             ui::Color::Hex(0x0E1420), 2.f, aura, 0.8f);
        Fill(ctx, ui::Rect(cx + 2.f * scale, feetY - 30.f * scale, 8.f * scale, 28.f * scale),
             ui::Color::Hex(0x0E1420), 2.f, aura, 0.8f);
        // Torso
        Fill(ctx, ui::Rect(cx - 12.f * scale, feetY - 58.f * scale, 24.f * scale, 28.f * scale),
             ui::Color::Hex(0x121A28), 3.f, aura, 1.f);
        // Head
        Fill(ctx, ui::Rect(cx - 8.f * scale, feetY - 74.f * scale, 16.f * scale, 15.f * scale),
             ui::Color::Hex(0x141E2E), 3.f, aura, 1.f);
    };

    // Player 1: Primary Target (Notch)
    {
        const float cx = 220.f, cy = 190.f;

        drawPlayerSilhouette(cx, cy + 90.f, 1.0f, ui::Color(244, 63, 94, 90));

        // 1. Equipped Armor + Hand slots row above the nametag
        const char* slots[] = {"HELM", "CHEST", "LEGS", "BOOTS", "SWD"};
        const char* ench[]  = {"P4 U3", "P4 U3", "P4 U3", "P4 U3", "S5 F2"};
        const float slotW = 34.f, slotH = 34.f, slotGap = 5.f;
        const float totalArmorW = 5 * slotW + 4 * slotGap;
        float ax = cx - totalArmorW * 0.5f, ay = cy - 64.f;

        for (int i = 0; i < 5; ++i) {
            ui::Rect sr(ax, ay, slotW, slotH);
            Fill(ctx, sr, ui::Color::Hex(0x0E1522), 4.f, ui::Color(56, 189, 248, 80), 1.f);
            ctx.DrawText(ui::Vec2(sr.x + 4.f, sr.y + 4.f), slots[i], ui::Color::Hex(0xE2E8F0), 9.f, nullptr, 0.2f);
            ctx.DrawText(ui::Vec2(sr.x + 3.f, sr.y + 16.f), ench[i], ui::Color::Hex(0x38BDF8), 8.5f, nullptr, 0.3f);
            // Durability bar at bottom of slot
            Fill(ctx, ui::Rect(sr.x + 2.f, sr.Max().y - 4.f, (sr.w - 4.f) * (i == 4 ? 0.65f : 0.90f), 2.f),
                 ui::Color::Hex(0x22C55E), 1.f);
            ax += slotW + slotGap;
        }

        // 2. Floating Nametag Card
        const float nw = 230.f, nh = 28.f;
        ui::Rect tag(cx - nw * 0.5f, cy - 22.f, nw, nh);
        ui::RectStyle ts;
        ts.fill = ui::Color::Hex(0x0C1322, 245);
        ts.cornerRadii = ui::Vec4(6.f);
        ts.borderColor = ui::Color(56, 189, 248, 100);
        ts.borderWidth = 1.2f;
        ts.shadowColor = ui::Color::Black();
        ts.shadowBlur = 14.f;
        ts.shadowOpacity = 0.5f;
        ctx.DrawRect(tag, ts);

        ctx.DrawText(ui::Vec2(tag.x + 8.f, tag.y + 6.f), "[MVP+]", ui::Color::Hex(0xF59E0B), 11.5f, nullptr, 0.4f);
        const float rw = ctx.MeasureText("[MVP+]", 11.5f).x;
        ctx.DrawText(ui::Vec2(tag.x + 12.f + rw, tag.y + 5.5f), "Notch", ui::Color::White(), 13.5f, nullptr, 0.35f);
        const float nwTxt = ctx.MeasureText("Notch", 13.5f).x;
        ctx.DrawText(ui::Vec2(tag.x + 16.f + rw + nwTxt, tag.y + 7.f), "12m", ui::Color::Hex(0x94A3B8), 11.f);
        ctx.DrawText(ui::Vec2(tag.x + 44.f + rw + nwTxt, tag.y + 7.f), "14ms", ui::Color::Hex(0x10B981), 11.f);
        ctx.DrawText(ui::Vec2(tag.Max().x - 52.f, tag.y + 6.f), "20.0 HP", ui::Color::Hex(0xEF4444), 11.5f, nullptr, 0.35f);

        // Health bar attached to bottom of nametag
        Fill(ctx, ui::Rect(tag.x, tag.Max().y, tag.w, 3.5f), ui::Color::Hex(0x162032), 0.f);
        Fill(ctx, ui::Rect(tag.x, tag.Max().y, tag.w * 1.0f, 3.5f), ui::Color::Hex(0x10B981), 1.5f);
    }

    // Player 2: Teammate / Friend (Alex)
    {
        const float cx = 490.f, cy = 140.f;

        drawPlayerSilhouette(cx, cy + 80.f, 0.88f, ui::Color(6, 182, 212, 80));

        const char* slots[] = {"HELM", "CHEST", "LEGS", "SWD"};
        const float slotW = 34.f, slotH = 32.f, slotGap = 4.f;
        const float totalArmorW = 4 * slotW + 3 * slotGap;
        float ax = cx - totalArmorW * 0.5f, ay = cy - 58.f;

        for (int i = 0; i < 4; ++i) {
            ui::Rect sr(ax, ay, slotW, slotH);
            Fill(ctx, sr, ui::Color::Hex(0x0A1624), 4.f, ui::Color(6, 182, 212, 60), 1.f);
            ctx.DrawText(ui::Vec2(sr.x + 4.f, sr.y + 3.f), slots[i], ui::Color::Hex(0xCBD5E1), 8.5f);
            ctx.DrawText(ui::Vec2(sr.x + 4.f, sr.y + 14.f), "P3", ui::Color::Hex(0x06B6D4), 8.5f, nullptr, 0.2f);
            Fill(ctx, ui::Rect(sr.x + 2.f, sr.Max().y - 3.5f, sr.w - 4.f, 2.f), ui::Color::Hex(0x22C55E), 1.f);
            ax += slotW + slotGap;
        }

        const float nw = 195.f, nh = 26.f;
        ui::Rect tag(cx - nw * 0.5f, cy - 18.f, nw, nh);
        ui::RectStyle ts;
        ts.fill = ui::Color::Hex(0x0A1624, 240);
        ts.cornerRadii = ui::Vec4(5.f);
        ts.borderColor = ui::Color(6, 182, 212, 110);
        ts.borderWidth = 1.f;
        ctx.DrawRect(tag, ts);

        ctx.DrawText(ui::Vec2(tag.x + 8.f, tag.y + 5.5f), "[Friend]", ui::Color::Hex(0x06B6D4), 11.f, nullptr, 0.35f);
        const float rw = ctx.MeasureText("[Friend]", 11.f).x;
        ctx.DrawText(ui::Vec2(tag.x + 12.f + rw, tag.y + 5.f), "Alex", ui::Color::Hex(0xE0F2FE), 13.f, nullptr, 0.3f);
        ctx.DrawText(ui::Vec2(tag.x + 58.f + rw, tag.y + 6.5f), "24m", ui::Color::Hex(0x94A3B8), 10.5f);
        ctx.DrawText(ui::Vec2(tag.Max().x - 48.f, tag.y + 5.5f), "18.5 HP", ui::Color::Hex(0x22C55E), 11.f, nullptr, 0.3f);

        Fill(ctx, ui::Rect(tag.x, tag.Max().y, tag.w, 3.f), ui::Color::Hex(0x132338), 0.f);
        Fill(ctx, ui::Rect(tag.x, tag.Max().y, tag.w * 0.925f, 3.f), ui::Color::Hex(0x22C55E), 1.5f);
    }

    // Player 3: Distant Opponent (Steve)
    {
        const float cx = 110.f, cy = 90.f;

        drawPlayerSilhouette(cx, cy + 65.f, 0.72f, ui::Color(245, 158, 11, 70));

        // Steve armor slots (complete consistency)
        const char* slots[] = {"HELM", "CHEST", "SWD"};
        const float slotW = 32.f, slotH = 28.f, slotGap = 4.f;
        const float totalArmorW = 3 * slotW + 2 * slotGap;
        float ax = cx - totalArmorW * 0.5f, ay = cy - 50.f;

        for (int i = 0; i < 3; ++i) {
            ui::Rect sr(ax, ay, slotW, slotH);
            Fill(ctx, sr, ui::Color::Hex(0x0C121E), 4.f, ui::Color(255, 255, 255, 25), 1.f);
            ctx.DrawText(ui::Vec2(sr.x + 4.f, sr.y + 2.f), slots[i], ui::Color::Hex(0xCBD5E1), 8.f);
            ctx.DrawText(ui::Vec2(sr.x + 4.f, sr.y + 12.f), "P2", ui::Color::Hex(0xF59E0B), 8.f);
            Fill(ctx, ui::Rect(sr.x + 2.f, sr.Max().y - 3.f, sr.w - 4.f, 2.f), ui::Color::Hex(0xF59E0B), 1.f);
            ax += slotW + slotGap;
        }

        const float nw = 170.f, nh = 24.f;
        ui::Rect tag(cx - nw * 0.5f, cy - 14.f, nw, nh);
        ui::RectStyle ts;
        ts.fill = ui::Color::Hex(0x0C121E, 230);
        ts.cornerRadii = ui::Vec4(5.f);
        ts.borderColor = ui::Color(255, 255, 255, 30);
        ts.borderWidth = 0.8f;
        ctx.DrawRect(tag, ts);

        ctx.DrawText(ui::Vec2(tag.x + 6.f, tag.y + 5.f), "[VIP]", ui::Color::Hex(0x22C55E), 10.5f, nullptr, 0.3f);
        const float rw = ctx.MeasureText("[VIP]", 10.5f).x;
        ctx.DrawText(ui::Vec2(tag.x + 10.f + rw, tag.y + 4.5f), "Steve", ui::Color::White(), 12.f, nullptr, 0.25f);
        ctx.DrawText(ui::Vec2(tag.x + 52.f + rw, tag.y + 6.f), "38m", ui::Color::Hex(0x94A3B8), 10.f);
        ctx.DrawText(ui::Vec2(tag.Max().x - 44.f, tag.y + 5.f), "14.2 HP", ui::Color::Hex(0xF59E0B), 10.5f, nullptr, 0.25f);

        Fill(ctx, ui::Rect(tag.x, tag.Max().y, tag.w, 2.5f), ui::Color::Hex(0x162032), 0.f);
        Fill(ctx, ui::Rect(tag.x, tag.Max().y, tag.w * 0.71f, 2.5f), ui::Color::Hex(0xF59E0B), 1.f);
    }
}

void SceneEsp(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    Fill(ctx, ui::Rect(0, 0, w, h), ui::Color::Hex(0x080C14));

    // Ground perspective grid ellipses
    ctx.DrawCircle(ui::Vec2(w * 0.5f, h * 0.90f), 220.f, ui::Color(255, 255, 255, 7), 1.f);
    ctx.DrawCircle(ui::Vec2(w * 0.5f, h * 0.90f), 140.f, ui::Color(255, 255, 255, 10), 1.f);

    auto drawCornerBrackets = [&](ui::Rect b, float arm, ui::Color col, float thick) {
        // Top-left
        ctx.DrawLine(ui::Vec2(b.x, b.y), ui::Vec2(b.x + arm, b.y), thick, col);
        ctx.DrawLine(ui::Vec2(b.x, b.y), ui::Vec2(b.x, b.y + arm), thick, col);
        // Top-right
        ctx.DrawLine(ui::Vec2(b.Max().x, b.y), ui::Vec2(b.Max().x - arm, b.y), thick, col);
        ctx.DrawLine(ui::Vec2(b.Max().x, b.y), ui::Vec2(b.Max().x, b.y + arm), thick, col);
        // Bottom-left
        ctx.DrawLine(ui::Vec2(b.x, b.Max().y), ui::Vec2(b.x + arm, b.Max().y), thick, col);
        ctx.DrawLine(ui::Vec2(b.x, b.Max().y), ui::Vec2(b.x, b.Max().y - arm), thick, col);
        // Bottom-right
        ctx.DrawLine(ui::Vec2(b.Max().x, b.Max().y), ui::Vec2(b.Max().x - arm, b.Max().y), thick, col);
        ctx.DrawLine(ui::Vec2(b.Max().x, b.Max().y), ui::Vec2(b.Max().x, b.Max().y - arm), thick, col);
    };

    // Target Player (Notch, Center-Right)
    {
        const ui::Rect b(260.f, 85.f, 150.f, 240.f);
        const float cx = b.Center().x;

        // Ground shadow
        Fill(ctx, ui::Rect(cx - 36.f, b.Max().y + 2.f, 72.f, 8.f), ui::Color(0, 0, 0, 200), 4.f);

        // Translucent player body mesh (gives authentic substance behind skeleton)
        Fill(ctx, ui::Rect(b.x + 20.f, b.y + 20.f, b.w - 40.f, b.h - 30.f),
             ui::Color::Hex(0x101726, 120), 4.f, ui::Color(244, 63, 94, 50), 1.f);

        // 1. 2D Corner L-Brackets
        drawCornerBrackets(b, 20.f, ui::Color::Hex(0xF43F5E), 2.f);

        // 2. Health Bar (Left edge) with clean attached HP label
        Fill(ctx, ui::Rect(b.x - 10.f, b.y, 4.f, b.h), ui::Color::Hex(0x111827), 1.5f);
        Fill(ctx, ui::Rect(b.x - 10.f, b.Max().y - b.h * 0.85f, 4.f, b.h * 0.85f), ui::Color::Hex(0x10B981), 1.5f);
        ctx.DrawText(ui::Vec2(b.x - 42.f, b.y + 4.f), "17 HP", ui::Color::Hex(0x10B981), 10.5f, nullptr, 0.35f);

        // 3. Armor Bar (Bottom edge)
        Fill(ctx, ui::Rect(b.x, b.Max().y + 5.f, b.w, 3.5f), ui::Color::Hex(0x111827), 1.5f);
        Fill(ctx, ui::Rect(b.x, b.Max().y + 5.f, b.w * 0.95f, 3.5f), ui::Color::Hex(0x06B6D4), 1.5f);

        // 4. Skeleton Bones & Joints
        const ui::Color skCol = ui::Color::Hex(0xF1F5F9);
        const ui::Color jCol = ui::Color::Hex(0x00F2FE);

        // Head
        ctx.DrawCircle(ui::Vec2(cx, b.y + 24.f), 12.f, skCol);
        // Spine
        ctx.DrawLine(ui::Vec2(cx, b.y + 36.f), ui::Vec2(cx, b.y + 110.f), 1.6f, skCol);
        // Shoulders
        ctx.DrawLine(ui::Vec2(cx - 32.f, b.y + 50.f), ui::Vec2(cx + 32.f, b.y + 50.f), 1.6f, skCol);
        // Left arm
        ctx.DrawLine(ui::Vec2(cx - 32.f, b.y + 50.f), ui::Vec2(cx - 40.f, b.y + 82.f), 1.5f, skCol);
        ctx.DrawLine(ui::Vec2(cx - 40.f, b.y + 82.f), ui::Vec2(cx - 36.f, b.y + 115.f), 1.5f, skCol);
        // Right arm
        ctx.DrawLine(ui::Vec2(cx + 32.f, b.y + 50.f), ui::Vec2(cx + 40.f, b.y + 82.f), 1.5f, skCol);
        ctx.DrawLine(ui::Vec2(cx + 40.f, b.y + 82.f), ui::Vec2(cx + 36.f, b.y + 115.f), 1.5f, skCol);
        // Pelvis
        ctx.DrawLine(ui::Vec2(cx - 20.f, b.y + 110.f), ui::Vec2(cx + 20.f, b.y + 110.f), 1.6f, skCol);
        // Left leg
        ctx.DrawLine(ui::Vec2(cx - 20.f, b.y + 110.f), ui::Vec2(cx - 24.f, b.y + 168.f), 1.5f, skCol);
        ctx.DrawLine(ui::Vec2(cx - 24.f, b.y + 168.f), ui::Vec2(cx - 26.f, b.y + 232.f), 1.5f, skCol);
        // Right leg
        ctx.DrawLine(ui::Vec2(cx + 20.f, b.y + 110.f), ui::Vec2(cx + 24.f, b.y + 168.f), 1.5f, skCol);
        ctx.DrawLine(ui::Vec2(cx + 24.f, b.y + 168.f), ui::Vec2(cx + 26.f, b.y + 232.f), 1.5f, skCol);

        // Articulation dots
        const ui::Vec2 joints[] = {
            {cx - 32.f, b.y + 50.f}, {cx + 32.f, b.y + 50.f},
            {cx - 40.f, b.y + 82.f}, {cx + 40.f, b.y + 82.f},
            {cx - 36.f, b.y + 115.f}, {cx + 36.f, b.y + 115.f},
            {cx - 24.f, b.y + 168.f}, {cx + 24.f, b.y + 168.f},
            {cx - 26.f, b.y + 232.f}, {cx + 26.f, b.y + 232.f}
        };
        for (const auto& j : joints) ctx.DrawCircle(j, 2.2f, jCol);

        // 5. Nametag and holding info placed ABOVE player (no tracer or bracket collision!)
        const ui::Rect topTag(cx - 70.f, b.y - 38.f, 140.f, 24.f);
        Fill(ctx, topTag, ui::Color::Hex(0x0C1322, 230), 4.f, ui::Color(244, 63, 94, 80), 1.f);
        ctx.DrawText(ui::Vec2(topTag.x + 8.f, topTag.y + 5.f), "[12.4m] Notch", ui::Color::White(), 11.5f, nullptr, 0.35f);
        ctx.DrawText(ui::Vec2(topTag.Max().x - 48.f, topTag.y + 5.5f), "SWORD", ui::Color::Hex(0x38BDF8), 9.5f, nullptr, 0.2f);

        // 6. Tracer Snapline from screen bottom-center to target's feet (NEVER crosses any text!)
        ctx.DrawLine(ui::Vec2(w * 0.5f, h), ui::Vec2(cx, b.Max().y + 12.f), 1.5f, ui::Color(244, 63, 94, 180));
    }

    // Distant Player (Steve, Left)
    {
        const ui::Rect b(60.f, 120.f, 96.f, 155.f);
        const float cx = b.Center().x;

        Fill(ctx, ui::Rect(cx - 22.f, b.Max().y + 2.f, 44.f, 6.f), ui::Color(0, 0, 0, 180), 3.f);
        Fill(ctx, ui::Rect(b.x + 12.f, b.y + 14.f, b.w - 24.f, b.h - 20.f),
             ui::Color::Hex(0x101726, 110), 3.f, ui::Color(245, 158, 11, 40), 1.f);

        drawCornerBrackets(b, 14.f, ui::Color::Hex(0xF59E0B), 1.5f);

        // Health bar & label
        Fill(ctx, ui::Rect(b.x - 7.f, b.y, 3.f, b.h), ui::Color::Hex(0x111827), 1.f);
        Fill(ctx, ui::Rect(b.x - 7.f, b.Max().y - b.h * 0.60f, 3.f, b.h * 0.60f), ui::Color::Hex(0xF59E0B), 1.f);
        ctx.DrawText(ui::Vec2(b.x - 38.f, b.y + 4.f), "12 HP", ui::Color::Hex(0xF59E0B), 9.5f);

        // Scaled skeleton with joints
        const ui::Color skCol = ui::Color::Hex(0xFDE68A);
        ctx.DrawCircle(ui::Vec2(cx, b.y + 16.f), 8.f, skCol);
        ctx.DrawLine(ui::Vec2(cx, b.y + 24.f), ui::Vec2(cx, b.y + 70.f), 1.2f, skCol);
        ctx.DrawLine(ui::Vec2(cx - 20.f, b.y + 33.f), ui::Vec2(cx + 20.f, b.y + 33.f), 1.2f, skCol);
        ctx.DrawLine(ui::Vec2(cx - 20.f, b.y + 33.f), ui::Vec2(cx - 24.f, b.y + 75.f), 1.2f, skCol);
        ctx.DrawLine(ui::Vec2(cx + 20.f, b.y + 33.f), ui::Vec2(cx + 24.f, b.y + 75.f), 1.2f, skCol);
        ctx.DrawLine(ui::Vec2(cx - 14.f, b.y + 70.f), ui::Vec2(cx + 14.f, b.y + 70.f), 1.2f, skCol);
        ctx.DrawLine(ui::Vec2(cx - 14.f, b.y + 70.f), ui::Vec2(cx - 16.f, b.y + 150.f), 1.2f, skCol);
        ctx.DrawLine(ui::Vec2(cx + 14.f, b.y + 70.f), ui::Vec2(cx + 16.f, b.y + 150.f), 1.2f, skCol);

        // Articulation dots
        ctx.DrawCircle(ui::Vec2(cx - 20.f, b.y + 33.f), 1.8f, ui::Color::Hex(0xF59E0B));
        ctx.DrawCircle(ui::Vec2(cx + 20.f, b.y + 33.f), 1.8f, ui::Color::Hex(0xF59E0B));
        ctx.DrawCircle(ui::Vec2(cx - 24.f, b.y + 75.f), 1.8f, ui::Color::Hex(0xF59E0B));
        ctx.DrawCircle(ui::Vec2(cx + 24.f, b.y + 75.f), 1.8f, ui::Color::Hex(0xF59E0B));

        // Nametag above player (no tracer or bracket collision!)
        const ui::Rect steveTag(cx - 50.f, b.y - 30.f, 100.f, 20.f);
        Fill(ctx, steveTag, ui::Color::Hex(0x0C121E, 220), 3.f, ui::Color(245, 158, 11, 70), 1.f);
        ctx.DrawText(ui::Vec2(steveTag.x + 6.f, steveTag.y + 4.f), "[28.0m] Steve", ui::Color::Hex(0xFDE68A), 10.f);

        // Tracer line to Steve's feet
        ctx.DrawLine(ui::Vec2(w * 0.5f, h), ui::Vec2(cx, b.Max().y + 8.f), 1.2f, ui::Color(245, 158, 11, 140));
    }
}

void SceneHud(ui::Context& ctx, aris_demo::DemoState&) {
    const float w = ctx.size().x, h = ctx.size().y;
    Fill(ctx, ui::Rect(0, 0, w, h), ui::Color::Hex(0x080C14));

    // 1. Watermark (Top-Left)
    DrawWatermark(ctx, 20.f, 18.f);

    // 2. Compact Arraylist (Top-Right)
    DrawArraylist(ctx, w - 20.f, 18.f, 6);

    // 3. Notification Toast (Below Target HUD on left margin)
    {
        const ui::Rect toast(20.f, 180.f, 255.f, 30.f);
        ui::RectStyle ts;
        ts.fill = ui::Color::Hex(0x0D1424, 240);
        ts.cornerRadii = ui::Vec4(15.f);
        ts.borderColor = ui::Color(0, 242, 254, 70);
        ts.borderWidth = 1.f;
        ts.shadowColor = ui::Color::Black();
        ts.shadowBlur = 12.f;
        ts.shadowOpacity = 0.4f;
        ctx.DrawRect(toast, ts);

        ui::DrawIcon(ctx, ui::Icon::Bolt, ui::Rect(toast.x + 10.f, toast.y + 7.f, 16.f, 16.f),
                     ui::Color::Hex(0x00F2FE), 1.6f);
        ctx.DrawText(ui::Vec2(toast.x + 32.f, toast.y + 7.5f),
                     "AutoArmor: Equipped P4 Chestplate", ui::Color::Hex(0xE2E8F0), 11.f);
    }

    // 4. Center-screen live PvP Target (Notch in combat view!)
    {
        const float cx = 390.f, cy = 190.f;

        // Ground shadow
        Fill(ctx, ui::Rect(cx - 30.f, cy + 96.f, 60.f, 8.f), ui::Color(0, 0, 0, 180), 4.f);

        // Stylized target silhouette
        Fill(ctx, ui::Rect(cx - 9.f, cy + 62.f, 8.f, 34.f), ui::Color::Hex(0x101726), 2.f, ui::Color(244, 63, 94, 90), 1.f);
        Fill(ctx, ui::Rect(cx + 1.f, cy + 62.f, 8.f, 34.f), ui::Color::Hex(0x101726), 2.f, ui::Color(244, 63, 94, 90), 1.f);
        Fill(ctx, ui::Rect(cx - 14.f, cy + 28.f, 28.f, 34.f), ui::Color::Hex(0x141E30), 3.f, ui::Color(244, 63, 94, 100), 1.f);
        Fill(ctx, ui::Rect(cx - 9.f, cy + 9.f, 18.f, 18.f), ui::Color::Hex(0x18243A), 3.f, ui::Color(244, 63, 94, 100), 1.f);

        // 2D Corner target brackets
        const ui::Rect tb(cx - 26.f, cy + 6.f, 52.f, 94.f);
        const float arm = 12.f;
        ctx.DrawLine(ui::Vec2(tb.x, tb.y), ui::Vec2(tb.x + arm, tb.y), 1.5f, ui::Color::Hex(0xF43F5E));
        ctx.DrawLine(ui::Vec2(tb.x, tb.y), ui::Vec2(tb.x, tb.y + arm), 1.5f, ui::Color::Hex(0xF43F5E));
        ctx.DrawLine(ui::Vec2(tb.Max().x, tb.y), ui::Vec2(tb.Max().x - arm, tb.y), 1.5f, ui::Color::Hex(0xF43F5E));
        ctx.DrawLine(ui::Vec2(tb.Max().x, tb.y), ui::Vec2(tb.Max().x, tb.y + arm), 1.5f, ui::Color::Hex(0xF43F5E));
        ctx.DrawLine(ui::Vec2(tb.x, tb.Max().y), ui::Vec2(tb.x + arm, tb.Max().y), 1.5f, ui::Color::Hex(0xF43F5E));
        ctx.DrawLine(ui::Vec2(tb.x, tb.Max().y), ui::Vec2(tb.x, tb.Max().y - arm), 1.5f, ui::Color::Hex(0xF43F5E));
        ctx.DrawLine(ui::Vec2(tb.Max().x, tb.Max().y), ui::Vec2(tb.Max().x - arm, tb.Max().y), 1.5f, ui::Color::Hex(0xF43F5E));
        ctx.DrawLine(ui::Vec2(tb.Max().x, tb.Max().y), ui::Vec2(tb.Max().x, tb.Max().y - arm), 1.5f, ui::Color::Hex(0xF43F5E));

        // Floating nametag above in-game entity
        const ui::Rect fTag(cx - 70.f, tb.y - 24.f, 140.f, 20.f);
        Fill(ctx, fTag, ui::Color::Hex(0x0C1322, 230), 4.f, ui::Color(244, 63, 94, 80), 1.f);
        ctx.DrawText(ui::Vec2(fTag.x + 6.f, fTag.y + 4.f), "[MVP+] Notch", ui::Color::White(), 10.5f, nullptr, 0.35f);
        ctx.DrawText(ui::Vec2(fTag.Max().x - 42.f, fTag.y + 4.5f), "3.4m", ui::Color::Hex(0x94A3B8), 9.5f);
    }

    // 5. Center-Left Target HUD Card (Locked onto Notch!)
    {
        const ui::Rect card(20.f, 76.f, 275.f, 92.f);
        ui::RectStyle cs;
        cs.fill = ui::Color::Hex(0x0C1322, 245);
        cs.cornerRadii = ui::Vec4(10.f);
        cs.borderColor = ui::Color(56, 189, 248, 80);
        cs.borderWidth = 1.f;
        cs.shadowColor = ui::Color::Black();
        cs.shadowBlur = 18.f;
        cs.shadowOpacity = 0.5f;
        ctx.DrawRect(card, cs);

        // Avatar tile with face icon
        const ui::Rect av(card.x + 12.f, card.y + 14.f, 54.f, 54.f);
        Fill(ctx, av, ui::Color::Hex(0x162032), 8.f, ui::Color(56, 189, 248, 100), 1.f);
        ui::DrawIcon(ctx, ui::Icon::Player, ui::Rect(av.x + 9.f, av.y + 9.f, 36.f, 36.f),
                     ui::Color::Hex(0x38BDF8), 1.8f);

        // Player Name & Rank
        ctx.DrawText(ui::Vec2(card.x + 76.f, card.y + 14.f), "Notch", ui::Color::White(), 15.f, nullptr, 0.4f);
        const float nw = ctx.MeasureText("Notch", 15.f).x;
        ctx.DrawText(ui::Vec2(card.x + 80.f + nw, card.y + 16.f), "[MVP+]", ui::Color::Hex(0xF59E0B), 11.5f, nullptr, 0.3f);

        // Win badge
        const ui::Rect winBadge(card.Max().x - 64.f, card.y + 14.f, 52.f, 18.f);
        Fill(ctx, winBadge, ui::Color::Hex(0x064E3B, 220), 4.f, ui::Color::Hex(0x10B981), 1.f);
        ctx.DrawText(ui::Vec2(winBadge.x + 6.f, winBadge.y + 3.f), "WIN 78%", ui::Color::Hex(0x6EE7B7), 9.5f, nullptr, 0.3f);

        // Stats line (strictly separated)
        ctx.DrawText(ui::Vec2(card.x + 76.f, card.y + 36.f), "Dist: 3.4m  |  Ping: 12ms",
                     ui::Color::Hex(0x94A3B8), 11.f);

        // Health row: Label on right, track across bottom
        ctx.DrawText(ui::Vec2(card.Max().x - 84.f, card.y + 54.f), "17.0 / 20.0 HP",
                     ui::Color::Hex(0xE2E8F0), 10.f, nullptr, 0.2f);

        const ui::Rect hb(card.x + 76.f, card.y + 68.f, card.w - 88.f, 9.f);
        Fill(ctx, hb, ui::Color::Hex(0x162032), 4.f);
        ui::RectStyle hbs;
        hbs.fill = ui::Color::Hex(0x10B981);
        hbs.fillEnd = ui::Color::Hex(0x06B6D4);
        hbs.gradientDir = ui::Vec2(1.f, 0.f);
        hbs.cornerRadii = ui::Vec4(4.f);
        ctx.DrawRect(ui::Rect(hb.x, hb.y, hb.w * 0.85f, hb.h), hbs);
    }

    // 6. Active Potion Effects Card (Bottom-Right)
    {
        const ui::Rect potCard(w - 170.f, h - 132.f, 150.f, 112.f);
        ui::RectStyle pcs;
        pcs.fill = ui::Color::Hex(0x0C121E, 235);
        pcs.cornerRadii = ui::Vec4(8.f);
        pcs.borderColor = ui::Color(255, 255, 255, 18);
        pcs.borderWidth = 1.f;
        ctx.DrawRect(potCard, pcs);

        ctx.DrawText(ui::Vec2(potCard.x + 10.f, potCard.y + 10.f), "ACTIVE POTIONS",
                     ui::Color::Hex(0x64748B), 10.5f, nullptr, 0.35f);

        auto drawPot = [&](float py, const char* name, const char* time, ui::Color col) {
            ctx.DrawText(ui::Vec2(potCard.x + 10.f, py), name, col, 12.f, nullptr, 0.2f);
            ctx.DrawText(ui::Vec2(potCard.Max().x - 42.f, py), time, ui::Color::Hex(0x94A3B8), 11.f);
        };
        drawPot(potCard.y + 30.f, "Speed II", "02:45", ui::Color::Hex(0x38BDF8));
        drawPot(potCard.y + 54.f, "Strength II", "01:18", ui::Color::Hex(0xF43F5E));
        drawPot(potCard.y + 78.f, "Regeneration", "00:32", ui::Color::Hex(0xEC4899));
    }

    // 7. Bottom-Center Hotbar (All 9 slots filled, slot 0 selected)
    {
        const char* hotbarItems[] = {"SWD", "ROD", "BOW", "GAP", "PRL", "POT", "BLK", "AXE", "WTR"};
        const float slot = 32.f, gap = 4.f;
        const float hbW = 9 * slot + 8 * gap;
        const float hx = (w - hbW) * 0.5f, hy = h - slot - 18.f;
        Fill(ctx, ui::Rect(hx - 6.f, hy - 6.f, hbW + 12.f, slot + 12.f),
             ui::Color::Hex(0x070C14, 220), 8.f, ui::Color(255, 255, 255, 16), 1.f);

        for (int i = 0; i < 9; ++i) {
            ui::Rect r(hx + i * (slot + gap), hy, slot, slot);
            if (i == 0) {   // Active slot (Diamond Sword)
                ui::RectStyle as;
                as.fill = ui::Color::Hex(0x162032, 240);
                as.cornerRadii = ui::Vec4(6.f);
                as.borderColor = ui::Color::Hex(0x00F2FE);
                as.borderWidth = 1.5f;
                as.shadowColor = ui::Color::Hex(0x00F2FE);
                as.shadowBlur = 8.f;
                as.shadowOpacity = 0.5f;
                ctx.DrawRect(r, as);
                ctx.DrawText(ui::Vec2(r.x + 6.f, r.y + 9.f), hotbarItems[i], ui::Color::White(), 10.f, nullptr, 0.35f);
            } else {
                Fill(ctx, r, ui::Color::Hex(0x0E1420, 200), 5.f, ui::Color(255, 255, 255, 10), 0.8f);
                ctx.DrawText(ui::Vec2(r.x + 6.f, r.y + 9.f), hotbarItems[i], ui::Color::Hex(0x94A3B8), 9.5f);
            }
        }
    }
}

// --- mock ClickGUI ----------------------------------------------------------

constexpr ui::Color kAccent  = ui::Color::Hex(0x7FA8CC);
constexpr ui::Color kAccent2 = ui::Color::Hex(0xA9C6DE);
constexpr ui::Color kHair    = ui::Color::Hex(0x2A3644);

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
        Fill(ctx, ui::Rect(x, y, 16.f, 16.f), kAccent, 4.f);
        ctx.DrawLine(ui::Vec2(x + 4.f, y + 8.5f), ui::Vec2(x + 7.f, y + 12.f), 1.8f, ui::Color::Hex(0x0B0F14));
        ctx.DrawLine(ui::Vec2(x + 7.f, y + 12.f), ui::Vec2(x + 12.5f, y + 4.5f), 1.8f, ui::Color::Hex(0x0B0F14));
    } else {
        Fill(ctx, ui::Rect(x, y, 16.f, 16.f), ui::Color::Hex(0x1A222C), 4.f, kHair, 1.f);
    }
    ctx.DrawText(ui::Vec2(x + 24.f, y), label, ui::Color::Hex(0xE6EDF3), fs);
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
// Derived from Swiss button geometry in SceneButtons.
constexpr ui::Vec2 kFirstWidgetCenter(300.f, 94.f);

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
    {"panel",          "full canonical demo panel (all MVP widgets)",   ScenePanel, nullptr, 526, 536},
    {"buttons",        "Neo-brutalist Swiss editorial buttons (idle state)", SceneButtons, nullptr, 600, 330},
    {"buttons-hover",  "Neo-brutalist action button hovered",           SceneButtonsHover, [](ui::Context& c) {
        c.ProcessInput(ui::InputEvent{ui::MouseMoveEvent{kFirstWidgetCenter}});
    }, 600, 330},
    {"buttons-press",  "Neo-brutalist action button pressed down",      SceneButtonsPress, [](ui::Context& c) {
        c.ProcessInput(ui::InputEvent{ui::MouseMoveEvent{kFirstWidgetCenter}});
        c.ProcessInput(ui::InputEvent{ui::MouseButtonEvent{
            ui::MouseButton::Left, true, kFirstWidgetCenter}});
    }, 600, 330},
    {"toggles",        "macOS Sequoia clean frosted glass settings",    SceneToggles, nullptr, 560, 370},
    {"sliders",        "studio DAW pro audio rack console",             SceneSliders, nullptr, 580, 380},
    {"text",           "Bauhaus typographic specimen & MSDF ladder",    SceneText, nullptr, 680, 420},
    {"card",           "cyber-tactical sci-fi telemetry HUD deck",      SceneCard, nullptr, 680, 440},
    {"animation",      "spring physics, bounce curves & pulsing radar", SceneAnimation, nullptr, 680, 450},
    {"hud",            "competitive client HUD: target card + hotbar",  SceneHud, nullptr, 700, 460},
    {"arraylist",      "competitive module arraylist (top-right)",      SceneArraylist, nullptr, 560, 380},
    {"nametags",       "competitive floating nametags with armor slots",SceneNametags, nullptr, 640, 360},
    {"esp",            "2D corner-box ESP with skeleton bones & tracer",SceneEsp, nullptr, 580, 420},
    {"clickgui",       "flagship: Aurora glass ClickGUI (kit widgets, icons, glow)", aurora::Scene,
        [](ui::Context& c) {
            c.ProcessInput(ui::InputEvent{ui::MouseMoveEvent{aurora::HoverPoint()}});
        }, (int)aurora::kW, (int)aurora::kH},
    {"clickgui-cards", "mock ClickGUI: sidebar + module cards (flat style)", SceneClickGui, nullptr, 560, 680},
};

// --- harness ----------------------------------------------------------------

void PrintUsage() {
    std::printf(
        "aris scene previewer — headless UI snapshots & animation exporter\n\n"
        "usage:\n"
        "  aris_scene_preview --list\n"
        "  aris_scene_preview [--scene NAME[,NAME...]] [--all] [--out DIR]\n"
        "                     [--width W] [--height H] [--warmup N] [--font TTF]\n"
        "                     [--anim [NAMES]] [--gif] [--mp4] [--frames N] [--fps F]\n\n"
        "options:\n"
        "  --list            print scene names + descriptions, one per line\n"
        "  --scene NAMES     comma-separated scenes to render (default: all)\n"
        "  --all             render every scene\n"
        "  --out DIR         output directory (default: previews)\n"
        "  --width/--height  framebuffer size (default per scene or 560x720)\n"
        "  --warmup N        frames rendered before snapshot for easing (default 10)\n"
        "  --font TTF        font file (default: auto-discover system font)\n"
        "  --anim [NAMES]    render animated multi-frame sequence (comma-separated or 'all')\n"
        "  --gif             encode animated GIF via ffmpeg\n"
        "  --mp4             encode MP4 video via ffmpeg\n"
        "  --frames N        number of animation frames (default 60)\n"
        "  --fps F           animation framerate (default 60)\n"
        "\noutput: DIR/<scene>.png and optionally DIR/<scene>.gif / DIR/<scene>.mp4\n");
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> want;
    std::vector<std::string> animScenes;
    std::string outDir = "previews";
    std::string fontArg;
    int width = 560, height = 720, warmup = 10;
    int scale = 2;
    bool listOnly = false, all = false;
    bool makeGif = false, makeMp4 = false;
    int animFrames = 60;
    float animFps = 60.f;

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
        else if (a == "--scale") scale = std::max(1, std::atoi(next("2").c_str()));
        else if (a == "--warmup") warmup = std::atoi(next("10").c_str());
        else if (a == "--font") fontArg = next();
        else if (a == "--anim") {
            std::string arg = "";
            if (i + 1 < argc && argv[i + 1][0] != '-') arg = next();
            if (arg.empty() || arg == "all") animScenes.push_back("all");
            else {
                for (const auto& s : SplitList(arg)) animScenes.push_back(s);
            }
        } else if (a == "--gif") makeGif = true;
        else if (a == "--mp4") makeMp4 = true;
        else if (a == "--frames") animFrames = std::atoi(next("60").c_str());
        else if (a == "--fps") animFps = (float)std::atof(next("60").c_str());
        else if (a == "--help" || a == "-h") { PrintUsage(); return 0; }
        else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); PrintUsage(); return 2; }
    }

    if (!animScenes.empty() && !makeGif && !makeMp4) {
        makeGif = true;   // Default to GIF when --anim is specified
    }

    if (listOnly) {
        for (const Scene& s : kScenes)
            std::printf("%s\t%s\n", s.name, s.description);
        return 0;
    }

    if (!all && want.empty()) {
        if (!animScenes.empty()) {
            for (const auto& as : animScenes) {
                if (as != "all") want.push_back(as);
            }
        }
        if (want.empty()) want.assign({"panel"});   // sensible default
    }

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
    int winW = width, winH = height;
    for (const Scene& s : kScenes) { winW = std::max(winW, s.width); winH = std::max(winH, s.height); }
    GLFWwindow* window = glfwCreateWindow(winW, winH, "aris scene preview", nullptr, nullptr);
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

        const int sw = s.width > 0 ? s.width : width;
        const int sh = s.height > 0 ? s.height : height;
        glViewport(0, 0, sw, sh);   // scene occupies the window's bottom-left sw x sh

        bool animateThis = false;
        for (const std::string& a : animScenes) {
            if (a == "all" || a == s.name) animateThis = true;
        }

        // 1. Static snapshot render
        {
            ui::Context ctx;   // fresh context per scene: no state bleed
            ctx.SetTheme(ui::Theme::Default());
            ctx.SetFont(&font);

            for (int f = 0; f < warmup; ++f) {
                glClearColor(0.08f, 0.08f, 0.1f, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);   // fresh buffer each frame (no ghosting)
                if (s.inject) s.inject(ctx);   // simulated input settles over frames
                ctx.BeginFrame(1.f / 60.f, (float)sw, (float)sh);
                aris_demo::DemoState st;
                st.time = 0.5f;   // Settle half a second in
                st.frame = f;
                s.build(ctx, st);
                ctx.Render();
            }
            glFinish();

            std::vector<unsigned char> px((size_t)sw * sh * 4);
            glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            std::vector<unsigned char> flipped(px.size());
            const int stride = sw * 4;
            for (int y = 0; y < sh; ++y)
                std::memcpy(&flipped[(size_t)y * stride], &px[(size_t)(sh - 1 - y) * stride], stride);

            int outW = sw * scale;
            int outH = sh * scale;
            int outStride = outW * 4;
            std::vector<unsigned char> outPx;
            const unsigned char* writePtr = flipped.data();
            int writeW = sw, writeH = sh, writeStride = stride;

            if (scale > 1) {
                outPx.resize((size_t)outW * outH * 4);
                stbir_resize_uint8_srgb(flipped.data(), sw, sh, stride,
                                        outPx.data(), outW, outH, outStride,
                                        STBIR_RGBA);
                writePtr = outPx.data();
                writeW = outW;
                writeH = outH;
                writeStride = outStride;
            }

            const std::string path = outDir + "/" + s.name + ".png";
            if (!stbi_write_png(path.c_str(), writeW, writeH, 4, writePtr, writeStride)) {
                std::fprintf(stderr, "failed to write %s\n", path.c_str());
                ++failures;
                continue;
            }
            std::printf("%s\t%s\n", s.name, path.c_str());
        }

        // 2. Animated sequence export (if requested)
        if (animateThis && (makeGif || makeMp4)) {
            const std::string frameDir = outDir + "/.frames_" + s.name;
            std::filesystem::create_directories(frameDir);

            ui::Context ctx;
            ctx.SetTheme(ui::Theme::Default());
            ctx.SetFont(&font);

            const int stride = sw * 4;
            std::vector<unsigned char> px((size_t)sw * sh * 4);
            std::vector<unsigned char> flipped(px.size());

            for (int f = 0; f < animFrames; ++f) {
                const float t = (float)f / animFps;
                glClearColor(0.08f, 0.08f, 0.1f, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);
                if (s.inject) s.inject(ctx);
                ctx.BeginFrame(1.f / animFps, (float)sw, (float)sh);
                aris_demo::DemoState st;
                st.time = t;
                st.frame = f;
                s.build(ctx, st);
                ctx.Render();
                glFinish();

                glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                for (int y = 0; y < sh; ++y)
                    std::memcpy(&flipped[(size_t)y * stride], &px[(size_t)(sh - 1 - y) * stride], stride);

                char frameFile[512];
                std::snprintf(frameFile, sizeof(frameFile), "%s/frame_%04d.png", frameDir.c_str(), f);
                if (scale > 1) {
                    std::vector<unsigned char> outPx((size_t)sw * scale * sh * scale * 4);
                    stbir_resize_uint8_srgb(flipped.data(), sw, sh, stride,
                                            outPx.data(), sw * scale, sh * scale, sw * scale * 4,
                                            STBIR_RGBA);
                    stbi_write_png(frameFile, sw * scale, sh * scale, 4, outPx.data(), sw * scale * 4);
                } else {
                    stbi_write_png(frameFile, sw, sh, 4, flipped.data(), stride);
                }
            }

            if (makeGif) {
                const std::string gifPath = outDir + "/" + s.name + ".gif";
                char cmd[1024];
                std::snprintf(cmd, sizeof(cmd),
                    "ffmpeg -y -framerate %d -i \"%s/frame_%%04d.png\" "
                    "-filter_complex \"fps=%d,split[s0][s1];[s0]palettegen=max_colors=128[p];[s1][p]paletteuse=dither=bayer\" "
                    "\"%s\" > /dev/null 2>&1",
                    (int)animFps, frameDir.c_str(), std::min(30, (int)animFps), gifPath.c_str());
                int ret = std::system(cmd);
                if (ret == 0) {
                    std::printf("%s [gif]\t%s\n", s.name, gifPath.c_str());
                } else {
                    std::fprintf(stderr, "ffmpeg failed to encode GIF: %s\n", gifPath.c_str());
                    ++failures;
                }
            }

            if (makeMp4) {
                const std::string mp4Path = outDir + "/" + s.name + ".mp4";
                char cmd[1024];
                std::snprintf(cmd, sizeof(cmd),
                    "ffmpeg -y -framerate %d -i \"%s/frame_%%04d.png\" "
                    "-c:v libx264 -pix_fmt yuv420p -crf 18 \"%s\" > /dev/null 2>&1",
                    (int)animFps, frameDir.c_str(), mp4Path.c_str());
                int ret = std::system(cmd);
                if (ret == 0) {
                    std::printf("%s [mp4]\t%s\n", s.name, mp4Path.c_str());
                } else {
                    std::fprintf(stderr, "ffmpeg failed to encode MP4: %s\n", mp4Path.c_str());
                    ++failures;
                }
            }

            std::filesystem::remove_all(frameDir);
        }
    }

    font.Unload();   // GL texture must be deleted before glfwTerminate drops the context
    glfwDestroyWindow(window);
    glfwTerminate();
    return failures ? 1 : 0;
}