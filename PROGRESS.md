# aris-ui — Project Progress Tracker

Long-term roadmap for the aris-ui library. Each stage lists goals and acceptance
criteria; statuses are updated as work completes.

**Statuses:** `DONE` · `IN PROGRESS` · `PENDING` · `BLOCKED`

---

## Stage 0 — Project scaffold — `DONE`
- [x] Git repo, directory layout, MIT license, README
- [x] Root `CMakeLists.txt` with options `ARIS_UI_BUILD_EXAMPLES` / `ARIS_UI_BUILD_TOOLS`
- [x] Vendored pinned third-party deps (`third_party/`): yoga, glad2 (GL core 3.3), stb, msdfgen, nlohmann/json
- [x] GLFW fetched via FetchContent for the desktop runner only

## Stage 1 — Core math & events — `DONE`
- [x] `Vec2`, `Vec4`, `Rect`, `Color` (include/ui/core/math.hpp)
- [x] Easing library: `ExpSmooth`, `Lerp`, `EaseOutCubic`, `EaseOutBack`, cubic-bezier solver (include/ui/core/easing.hpp)
- [x] Generic `InputEvent` (`std::variant`): mouse move / button / scroll, key, text (include/ui/core/event.hpp)
- [x] Widget ID hashing + ID stack helpers (include/ui/core/id.hpp)
- **Acceptance:** pure C++, zero platform includes; compiles standalone.

## Stage 2 — Context, widget state & command queue — `DONE`
- [x] `Context` lifecycle: `BeginFrame(dt, w, h, scale)`, `ProcessInput(e) -> bool consumed`, `Render()`
- [x] Widget state store: per-ID `WidgetState{hoverT, pressT}` with exponential smoothing + stale-state GC
- [x] Frame rect registry (`prevRects` for input/draw, current frame filled after layout)
- [x] Input arbitration: press/release capture, `activeId`, click detection, consumed-event policy
- [x] POD command queue: `RoundedRect`, `Glyph`, `ClipPush/Pop` records
- **Acceptance:** full frame loop works headless (commands generated, no GL needed).

## Stage 3 — Yoga layout integration — `DONE`
- [x] `LayoutNode` RAII wrapper over vendored Yoga (flex styles, per-corner padding/margin, alignment)
- [x] Retained panels/groups: nodes reused across frames, stale children culled
- [x] `BeginPanel` / `EndPanel` absolute-positioned root + `BeginRow` / `BeginColumn` / `EndGroup` flex containers
- [x] Layout pass (`YGNodeCalculateLayout`) fills the current-frame rect registry
- **Acceptance:** two-column flexbox panel lays out correctly in the runner.

## Stage 4 — OpenGL SDF renderer — `DONE`
- [x] glad2 loader + `#version 330 core` shader pipeline (150-core/2.1 variants = Stage 9)
- [x] SDF fragment shader: per-corner `vec4` radii, antialiased fill (`fwidth`), inner border, soft drop shadow (offset/spread/blur/opacity), quad padding
- [x] MSDF text fragment path (3-channel median + screen-px-range scaling)
- [x] Premultiplied-alpha blending owned by the batcher for the UI pass (`GL_ONE, GL_ONE_MINUS_SRC_ALPHA`)
- [x] Dynamic interleaved vertex batcher: 1 draw call for rects, batched per atlas texture for glyphs
- [x] Scissor clipping via `ClipPush/Pop`, ortho projection with y-down pixel coords
- **Acceptance:** antialiased rounded rect with adjustable radius, border and shadow renders in the runner.

## Stage 5 — OpenGL state guard — `DONE`
- [x] RAII `GlStateGuard` snapshot/restore: program, VAO, VBO/IBO bindings, blend func+equation, cull/depth/scissor/viewport, texture bindings per unit, active texture, pixel store
- [x] Guard wraps all backend rendering; host pipeline verified unaffected (runner draws its own background pass before/after UI)
- **Acceptance:** host-owned GL state identical before/after `ctx.Render()`.

## Stage 6 — Font pipeline — `DONE`
- [x] Offline generator `tools/atlas_generator` (FreeType + msdfgen): MSDF glyphs, shelf packing with px-range padding, PNG + JSON metadata (em-normalized metrics)
- [x] Runtime `FontAtlas`: PNG + JSON only at runtime (stb_image + nlohmann), scale-invariant glyph quads, text measurement
- [x] Text rendering & clipping in widgets (UTF-8 decoded per codepoint, no shaping/kerning yet)
- **Acceptance:** crisp text at arbitrary sizes/DPI in the runner; `atlasgen` CLI produces atlases from any TTF.

## Stage 7 — Widget set & theming — `DONE`
- [x] Design-token `Theme` (Tailwind/Material-style palette, radii, shadows, spacing)
- [x] `Button` (accent fill, hover/press easing, click feedback)
- [x] `Toggle` (pill track + eased sliding knob)
- [x] `Slider` (accent fill, draggable knob, value label)
- [x] `Panel` (border, shadow, title) + two-column flexbox demo
- [x] Labels with hoverable value text
- **Acceptance:** interactive widgets with smooth color/scale transitions in the runner.

## Stage 8 — Desktop runner — `DONE`
- [x] GLFW 3.3 core window, event pump → `ui::InputEvent` translation
- [x] Host-owned background pass (proves state guard) + demo UI frame loop
- [x] Atlas auto-generation on startup (cached), CLI font override
- **Acceptance:** all 5 MVP deliverables demonstrable in one binary.

## Stage 8.5 — Agent tooling & docs — `DONE`
- [x] Headless scene preview tool `examples/scene_preview` → `build/aris_scene_preview`: 13 named scenes — widget set (panel, buttons/hover/press with simulated input, toggles, sliders, text, card) and mock-game overlays (hud with arraylist/ESP/nametags/hotbar, arraylist, nametags, esp, clickgui with sidebar + module cards), all resolution-independent
- [x] Shared demo bootstrap `examples/common/AppSupport.hpp` (font discovery, atlas bootstrap, canonical `BuildDemoUi`)
- [x] Zero-dependency test suite `tests/` (30 tests: math, easing, ids, command queue, widget state store, Yoga layout) wired into CMake/ctest
- [x] `Makefile`: configure / build / test / preview / demo / docs / verify / clean / help
- [x] `AGENTS.md`: repo map, verification loop, critical gotchas, conventions
- [x] Docs generator `tools/gen_docs.py` (stdlib-only md→html) + `docs-src/*.md` (7 pages)
- [x] Landing page `docs/index.html` + generated `docs/*.html`, `style.css`, `llms.txt`, `llms-full.txt`
- [x] Theme accent moved from violet to steel blue (`0x7FA8CC`); `✓` added to default atlas charset
- **Acceptance:** `make verify` = tests pass + previews render; docs site browsable from `docs/index.html`.

---

## Stage 9+ — Roadmap (pending)
- [ ] **Stage 9 — Visual effects:** dual-Kawase background blur via FBOs, glows, gradient fills, images/nine-patch
- [ ] **Stage 10 — Multi-version GL:** `#version 150 core` + GL 2.1 compatibility shader variants with runtime probe; explicit VAO fallback for GL≤2.1
- [ ] **Stage 11 — Input backends:** Win32 `WndProc` adapter, standalone GLFW adapter module; cursor grab/ungrab controller (LWJGL2 + GLFW modes)
- [ ] **Stage 12 — Minecraft helper layer:** GUI-scale synchronization (1x–4x/Auto), swapbuffer hook examples (`wglSwapBuffers`, `glXSwapBuffers`/`eglSwapBuffers`), minimal injected DLL example
- [ ] **Stage 13 — Text v2:** kerning/shaping (HarfBuzz optional), glyph cache with runtime re-raster, font fallbacks
- [ ] **Stage 14 — Widgets v2:** checkbox/dropdown/text-input/scrollview/tooltip, nesting, window manager (drag, z-order)
- [ ] **Stage 15 — Dev experience:** hot-reload of widget code/styles in runner, GPU timing overlay, screenshot tests
- [ ] **Stage 16 — Packaging:** prebuilt binaries, CMake `find_package(aris-ui)`

### Known MVP limitations (documented, by design)
- One-frame latency between layout changes and their first visible frame (egui-style).
- No kerning/shaping; per-codepoint glyph lookup.
- Retained child nodes are reused by insertion order, not by stable keys.
- Single font atlas per context.