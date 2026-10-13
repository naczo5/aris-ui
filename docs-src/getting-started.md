# Getting Started

aris-ui is a modern C++20 immediate-mode UI library for OpenGL. It renders
widgets as analytically anti-aliased signed-distance fields, draws text from
offline-generated MSDF atlases, lays out panels with Yoga flexbox, and leaves
the host's OpenGL state exactly as it found it.

## Requirements

- CMake 3.20+
- A C++20 compiler (GCC 11+, Clang 13+, MSVC 2019+)
- OpenGL 3.3 (GL 2.1 / 150-core variants are planned — Stage 10)
- FreeType (development package) for the atlas generator

## Build

```sh
git clone <repo> aris-ui && cd aris-ui
make configure     # = cmake -S . -B build -DARIS_UI_BUILD_TESTS=ON
make build         # = cmake --build build
make test          # unit tests (ctest)
make preview       # headless UI snapshots → previews/*.png
make demo          # interactive desktop runner window
```

No package hunting: Yoga, msdfgen, glad, stb and nlohmann/json are vendored
and pinned under `third_party/`. Only GLFW (for the examples) is fetched by
CMake at configure time.

## Your first frame

aris-ui is caller-driven: *you* own the frame loop and the GL context, the
library never touches a window, swaps a buffer, or starts a thread.

```cpp
#include "ui/aris_ui.hpp"
#include "atlas_generator/atlas_gen.hpp"

ui::Context ctx;                 // one per process
ui::FontAtlas font;
font.Load("atlas/ui.png", "atlas/ui.json");
ctx.SetTheme(ui::Theme::Default());
ctx.SetFont(&font);

// --- your render loop, every frame ---
ui::GlStateGuard guard;          // snapshots ALL relevant GL state
ctx.BeginFrame(dt, width, height);
ctx.ProcessInput(event);         // translated host events (see Core Concepts)

ctx.BeginPanel("main", ui::Rect(28, 28, 470, 640),
               ui::PanelStyle::FromTheme(ui::Theme::Default(), 18.f, "Hello"));
if (ui::Button(ctx, "Click me")) ++clicks;
ui::Toggle(ctx, "Enabled", &enabled);
ui::Slider(ctx, "Opacity", &opacity, 0.f, 1.f);
ctx.EndPanel();

ctx.Render();                    // layout + draw; guard restores host state
```

Generating the font atlas (offline, cached — regenerate only when the font
changes):

```cpp
std::string err;
ui_atlas::Params params;                 // fontSize 48, pxRange 4, ASCII + extras
ui_atlas::GenerateAndWrite("font.ttf", "atlas/ui", params, err);
```

Or from the command line: `./build/atlasgen <font.ttf> <out-prefix>`.

## Integration checklist

1. Translate your platform input into `ui::InputEvent` variants and feed each
   one to `ctx.ProcessInput()` before drawing.
2. Wrap `ctx.Render()` in a `ui::GlStateGuard` so your own pipeline state
   (blend, depth, VAOs, program) survives the UI pass untouched.
3. Draw the UI **last**, after your scene, with depth testing disabled.

## Next steps

- [Core Concepts](core-concepts.html) — the frame lifecycle and state model
- [Widgets](widgets.html) — the built-in control set and theming tokens
- [Layout](layout.html) — panels, rows, columns and flexbox styling
- [Rendering](rendering.html) — SDF shader contract and the batcher
- [Fonts](fonts.html) — the MSDF atlas pipeline and JSON schema
- [Agent Tooling](agent-tooling.html) — previews, tests and docs for coding agents