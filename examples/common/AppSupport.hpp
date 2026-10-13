// aris-ui — shared bootstrap helpers for the example binaries.
// Font discovery, atlas generation/caching, and the canonical demo UI live
// here so the desktop runner and the headless scene previewer stay in sync.
#pragma once

#include <cstdio>
#include <filesystem>
#include <string>

#include "ui/aris_ui.hpp"
#include "atlas_generator/atlas_gen.hpp"

namespace aris_demo {

// Common system font locations (Linux first; pass an explicit path to override).
inline std::string FindSystemFont() {
    namespace fs = std::filesystem;
    const char* candidates[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/Inter-Regular.ttf",
        "/usr/share/fonts/inter/Inter-Regular.ttf",
        "/usr/share/fonts/ttf-jetbrains-mono/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    };
    for (const char* c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec)) return c;
    }
    return {};
}

// Generates (or reuses cached) atlas files under <prefix>.png/.json.
// Returns false on failure (error text filled).
inline bool BootstrapAtlas(const std::string& fontPath, const std::string& prefix,
                           std::string& error) {
    namespace fs = std::filesystem;
    fs::create_directories(fs::path(prefix).parent_path().string());
    ui_atlas::Params params;
    return ui_atlas::GenerateAndWrite(fontPath, prefix, params, error);
}

// State behind the canonical demo panel (mutable by widget interactions).
struct DemoState {
    int clicks = 0;
    bool esp = true;
    bool glow = false;
    float opacity = 0.85f;
    float radius = 18.f;
    float shadow = 0.45f;
};

// The canonical two-column demo panel exercising every MVP deliverable:
// SDF panel/border/shadow, button, toggles, sliders, live-driven preview card.
inline void BuildDemoUi(ui::Context& ctx, DemoState& st) {
    const ui::Theme& th = ui::Theme::Default();
    const ui::PanelStyle panelStyle =
        ui::PanelStyle::FromTheme(th, 18.f, "ARIS UI — SDF panel");

    ctx.BeginPanel("main", ui::Rect(28, 28, 470, 640), panelStyle);

    ui::Label(ctx, "Modern game overlay toolkit", th.textDim, 13.f);

    ctx.BeginRow();
    {
        // --- column 1: controls ---
        ctx.BeginColumn();
        ui::Label(ctx, "Controls", th.text, 14.f);
        if (ui::Button(ctx, "Click me")) ++st.clicks;
        {
            static std::string clicksText;
            clicksText = "Clicks: " + std::to_string(st.clicks);
            ui::Label(ctx, clicksText.c_str(), th.textDim, 13.f);
        }
        ui::Toggle(ctx, "ESP", &st.esp);
        ui::Toggle(ctx, "Glow", &st.glow);
        ui::Slider(ctx, "Opacity", &st.opacity, 0.f, 1.f);
        ui::Slider(ctx, "Corner radius", &st.radius, 0.f, 28.f);
        ui::Slider(ctx, "Shadow", &st.shadow, 0.f, 1.f);
        ctx.EndGroup();

        // --- column 2: live preview ---
        ctx.BeginColumn();
        ui::Label(ctx, "Preview", th.text, 14.f);
        ui::WidgetFrame w = ctx.BeginWidget("__preview", 210.f);
        if (w.rect.Valid()) {
            const ui::Rect inner = w.rect.Inset(2.f);
            // card with live-driven SDF parameters
            ui::RectStyle st2;
            st2.fill = ui::Color::Hex(0x1C2128, (std::uint8_t)(st.opacity * 255.f));
            st2.cornerRadii = ui::Vec4(st.radius);
            st2.borderColor = th.controlBorder;
            st2.borderWidth = 1.f;
            st2.shadowOffset = ui::Vec2(0.f, 10.f);
            st2.shadowBlur = 24.f;
            st2.shadowOpacity = st.shadow;
            ctx.DrawRect(inner, st2);

            // accent pill inside the card
            ui::RectStyle pill;
            pill.fill = th.accent;
            pill.cornerRadii = ui::Vec4(12.f);
            pill.shadowOffset = ui::Vec2(0.f, 4.f);
            pill.shadowBlur = 12.f;
            pill.shadowOpacity = 0.35f * st.shadow;
            ctx.DrawRect(ui::Rect(inner.x + 14.f, inner.y + 16.f, 120.f, 24.f), pill);

            const char* line = "Rounded rect rendered";
            ctx.DrawText(ui::Vec2(inner.x + 14.f, inner.y + 56.f), line, th.textDim, 13.f, &inner);
            const char* line2 = "entirely in a fragment shader";
            ctx.DrawText(ui::Vec2(inner.x + 14.f, inner.y + 56.f + 18.f), line2, th.textDim, 13.f, &inner);
        }
        ctx.EndWidget();
        ctx.EndGroup();
    }
    ctx.EndGroup();

    ctx.EndPanel();
}

} // namespace aris_demo