# aris-ui

A lightweight, modern, drop-in C++ UI library — an aesthetic alternative to Dear ImGui
for game injection and overlay rendering, with a primary focus on Minecraft (1.8.9 through
modern releases) and other OpenGL-based applications.

Hardware-antialiased rounded corners, drop shadows, smooth animations and crisp typography
via a Multi-channel Signed Distance Field (MSDF) font pipeline — all rendered natively on
the GPU instead of through a multi-megabyte embedded web engine.

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

## Quick start (desktop runner)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/examples/desktop_runner/aris_runner
```

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