# aris-ui

**English** | [简体中文](README.zh-CN.md)

**A more beautiful ImGui replacement. Optimised for agents.**

A lightweight, modern, drop-in C++20 immediate-mode UI library for OpenGL overlay
injection — Minecraft (1.8.9 through modern releases) and any other OpenGL-based
application. Renders widgets as analytically anti-aliased signed-distance fields, draws
text from offline-generated MSDF atlases, lays out panels with Yoga flexbox, and leaves
the host's OpenGL state exactly as it found it. No web engine, no resource hogging, no
sloppy defaults.

## Preview

<p align="center">
  <img src="docs/assets/clickgui.png" width="49%" alt="Aurora glass ClickGUI with a translucent backdrop and module cards">
  <img src="docs/assets/hud.png" width="49%" alt="Competitive in-game HUD: target card, watermark, hotbar and active potions">
  <img src="docs/assets/panel.png" width="49%" alt="Widget panel with SDF buttons, toggles and sliders">
  <img src="docs/assets/text.png" width="49%" alt="MSDF type specimen rendered at several pixel sizes">
</p>

Every image above is a real framebuffer dump produced headlessly by `make preview`
(15 named scenes); the full gallery lives on the [docs landing page](docs/index.html).

## Tenets

1. **Library, not a framework.** Caller-driven execution: the host keeps ownership of the
   process lifecycle, event loop and swapchain. You call `ProcessInput()`, `BeginFrame()`,
   and `Render()` where *you* want.
2. **Decoupled 3-layer design.**
   - *Core Engine* — 100% platform-agnostic C++20 (math, layout tree, animations, widget
     state, command queue generation).
   - *Rendering Backend* — pure OpenGL Core Profile 3.2+ (SDF shaders, dynamic batching,
     defensive state guard), independent of the host engine's graphics pipelines.
   - *Platform / Input Layer* — standalone input adapters (Win32 `WndProc`, GLFW callbacks)
     feeding generic input events to the core.
3. **Fast standalone development loop.** A GLFW + OpenGL desktop runner for rapid
   prototyping without a running game or debugger injection.

## Quick start

```bash
git clone <repo> aris-ui && cd aris-ui
make configure        # = cmake -S . -B build -DARIS_UI_BUILD_TESTS=ON
make build            # = cmake --build build
make test             # unit tests (ctest)
make preview          # headless UI snapshots → previews/*.png
make demo             # interactive desktop runner window
```

The runner renders the canonical demo UI: rounded SDF panels, animated buttons,
toggles and sliders, MSDF text. For a pixel-accurate preview of every widget without
opening a window, `make preview` writes PNG snapshots you can view directly.

## Minimal usage (host integration)

```cpp
ui::Context ctx;
ui::FontAtlas font;
font.Load("atlas.png", "atlas.json");   // generated offline by tools/atlas_generator
ctx.SetFont(&font);

// per frame, on the thread that owns the GL context:
ctx.BeginFrame(dt, width, height, uiScale);
for (const auto& e : hostEvents)
    if (ctx.ProcessInput(e)) { /* block input from reaching the game */ }

if (ctx.BeginPanel("main", {16, 16, 380, 520}, ui::PanelStyle{})) {
    if (ui::Button(ctx, "Click me")) ++clicks;
    ui::Toggle(ctx, "Enable", &enabled);
    ui::Slider(ctx, "Opacity", &opacity, 0.f, 1.f);
}
ctx.EndPanel();

ctx.Render();   // flushes the command queue through the GL backend
```

## For agents (and humans who think like them)

- **[AGENTS.md](AGENTS.md)** — repo map, verification loop, and known gotchas.
- **`make preview`** — deterministic headless snapshots (`examples/scene_preview`,
  15 named scenes with simulated hover/press input) → `previews/*.png`. Render,
  then *look* at the PNGs.
- **`make test`** — 38 unit tests via ctest, no external test framework.
- **`docs/llms.txt`** and **`docs/llms-full.txt`** — agent-oriented doc indexes;
  every docs page also has a raw markdown source in `docs-src/`.

## Documentation

Full docs live in [`docs/`](docs/index.html) (GitHub Pages–ready) and as markdown in
[`docs-src/`](docs-src/): getting started, core concepts, widgets, layout, rendering,
fonts, agent tooling.

## Repository layout

```
├── include/ui/           # public headers (core, layout, renderer, font, widgets)
├── src/                  # implementation
├── tools/atlas_generator # offline MSDF atlas generator (FreeType + msdfgen)
├── examples/
│   ├── desktop_runner/   # standalone GLFW testbed
│   └── minecraft_hook/   # minimal injected-DLL example (future stage)
├── assets/               # fonts and generated atlases
└── third_party/          # pinned: yoga, glad, stb, msdfgen, nlohmann/json
```

See [PROGRESS.md](PROGRESS.md) for the staged roadmap and current status.

## License

MIT — see [LICENSE](LICENSE). Vendored third-party libraries carry their own licenses
(see `third_party/`).