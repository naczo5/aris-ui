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