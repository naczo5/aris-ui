# AGENTS.md — guidance for AI coding agents working on aris-ui

Modern C++20 immediate-mode UI library for OpenGL overlay injection
(ImGui-style, built for Minecraft client overlays and standalone GL apps).
This file tells you how to build, verify, and extend the project without
guessing. Human contributors benefit from the same workflow.

## Repo map

```
include/ui/            public API (headers are the contract)
  core/                math.hpp, easing.hpp, event.hpp, id.hpp, theme.hpp,
                       widget_state.hpp, context.hpp (the frame API)
  layout/              layout_node.hpp (RAII Yoga wrapper)
  renderer/            command_queue.hpp (POD draw commands), batcher.hpp,
                       draw_backend.hpp, vulkan_renderer.hpp, ui_vertex.hpp,
                       gl_state_guard.hpp
  font/                font_atlas.hpp (runtime MSDF atlas)
  widgets/             widgets.hpp (Button/Toggle/Slider/Label)
  aris_ui.hpp          umbrella header
src/                   implementations of the above
tools/atlas_generator/ offline MSDF atlas builder (FreeType + msdfgen 1.12)
examples/
  common/AppSupport.hpp  shared bootstrap: font discovery, atlas caching,
                         canonical demo UI (single source of truth)
  desktop_runner/        interactive GLFW demo window
  scene_preview/         HEADLESS snapshot tool → previews/*.png  ← agents use this
tests/                  zero-dependency test harness (tests/test_main.hpp) + suites
docs-src/               Markdown documentation sources
docs/                   GitHub Pages site (generated HTML + llms.txt)
third_party/            vendored, pinned: yoga v3.2.1, msdfgen v1.12.1, glad2,
                        stb, nlohmann/json  (GLFW fetched by CMake for examples)
```

## Commands (use the Makefile — never hand-roll build steps)

```sh
make configure   # first configure (also enables tests)
make build       # compile everything
make test        # build + ctest (unit tests, no GL needed)
make preview     # headless render of all UI scenes → previews/*.png
make demo        # interactive runner window
make docs        # regenerate docs/*.html from docs-src/*.md
make spv         # regenerate src/renderer/vk_spv.inc from the Vulkan GLSL
make verify      # build + test + preview — run after EVERY change
```

Binaries land in `build/`: `aris_tests`, `aris_scene_preview`, `aris_runner`,
`atlasgen`.

## The verification loop (do this after every change)

1. `make verify` — compiles, runs the test suite, renders all scenes.
2. **Look at the previews.** `previews/*.png` are real framebuffer dumps of
   named UI scenes (panel, buttons, buttons-hover, buttons-press, toggles,
   sliders, text, card, hud, arraylist, nametags, esp, clickgui). If you
   changed rendering, widgets, or layout, read
   the affected PNG (they are small, 560x720) and confirm it looks right.
3. Interactive behavior: `make demo` opens a window (needs a display).

Do not claim a rendering change works without having viewed a preview PNG.

## Critical gotchas (learned the hard way)

- **One-frame latency:** widget rects come from the *previous* frame's layout
  (egui-style). Input uses prev rects; don't "fix" this by reordering.
- **GL state ownership:** the batcher enables premultiplied blending
  (`GL_ONE, GL_ONE_MINUS_SRC_ALPHA`) and owns blend state for the UI pass.
  `GlStateGuard` saves/restores everything else around `ctx.Render()`.
- **msdfgen v1.12 semantics:** `pixel = scale * (shape + translate)` —
  translate is in SHAPE units; `Range` must be in the same units as the shape
  (em here): `Range(pxRange / fs)`. Don't "simplify" this back to pixels.
- **Atlas cache:** `atlas/ui.png|json` regenerate only when older than the
  font. Delete them after changing `tools/atlas_generator/`.
- **GL context lifetime:** `FontAtlas` stores CPU pixels. The GL batcher uploads
  on draw and deletes that texture in `Shutdown()`. Call `Context::ReleaseGraphics()`
  before the GL context dies. `Unload()` does not touch GL.
- **Vulkan:** `VulkanRenderer::Prepare(atlas)` uploads fonts and must run outside
  a render pass. `SetCommandBuffer()` then `Context::Render()` records inside the
  host pass. Backdrop blur is skipped. Functions come from the host loader; the
  library does not link `vulkan-1`. The pipeline is built for the `renderPass`,
  `subpass` and `sampleCount` from `VulkanCreateInfo`; call `Init()` again after
  the host recreates its render pass. `Draw()` rotates `framesInFlight`
  vertex/descriptor slots, so the host must not have more frames in flight than
  that.
- **Vulkan shaders live twice:** the OpenGL GLSL is the raw string in
  `src/renderer/batcher.cpp`, the Vulkan copy is `src/renderer/shaders/ui.vert|ui.frag`
  compiled into the checked-in `src/renderer/vk_spv.inc`. After editing either,
  run `make spv` (needs glslang in `tools/glslang/bin` + Python; CMake does not
  generate the `.inc`). `make spv CHECK=1` fails if the `.inc` is stale.
- **Scene previews:** each scene gets a fresh `ui::Context`; the harness
  clears the framebuffer every frame. Simulated input is injected *before*
  `BeginFrame` and eases over `--warmup` frames. Overlay scenes (hud,
  arraylist, nametags, esp, clickgui) draw with absolute-coordinate
  `ctx.DrawRect`/`DrawText` — no layout pass — and size off `ctx.size()`, so
  they render correctly at any `--width/--height`.
- **Atlas charset:** default glyphs are ASCII 32–126 plus extras (×, —, ✓)
  declared in `ui_atlas::Params::extraChars`. New non-ASCII glyphs require
  deleting `atlas/ui.png|json` (the cache) or extending `extraChars`.

## Conventions

- C++20, no exceptions across public API boundaries, no RTTI dependencies.
- Headers in `include/ui/` are the public contract — keep them clean of
  implementation details; implement in `src/`.
- Do **not** add third-party dependencies without asking. Vendored deps are
  trimmed + pinned; versions live in `third_party/VERSIONS.md`.
- Coordinate system: y-down pixels, origin top-left; colors are 8-bit RGBA
  (`ui::Color`), geometry in `ui::Rect/Vec2`.
- Widgets: immediate-mode calls that draw + handle input in one call, with
  eased transitions from `WidgetStateStore`. Follow `src/widgets/widgets.cpp`
  as the pattern.
- Text: UTF-8 in → per-codepoint glyphs out. No shaping/kerning yet (Stage 13).
- Update `PROGRESS.md` stage checkboxes when a stage's acceptance criteria
  change state. Update `docs-src/*.md` when public behavior changes, then
  `make docs`.

## Docs for agents

- Fetch-friendly docs live at `docs/llms.txt` (index) and `docs/llms-full.txt`
  (all pages concatenated) once the site is deployed; the Markdown sources in
  `docs-src/` are the same content, readable straight from the repo.
- Scene list for previews: `./build/aris_scene_preview --list`.

## Commit style

Small, topic-scoped commits; imperative subject ("Add …", "Fix …"). Include
generated previews only when they illustrate a deliberate visual change
(otherwise `previews/` is git-ignored).