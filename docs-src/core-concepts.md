# Core Concepts

aris-ui follows an immediate-mode *API over retained state* model: your code
declares the UI every frame like Dear ImGui, while the library retains the
expensive parts (widget state, Yoga layout tree, GPU batches) between frames.

## The frame lifecycle

```cpp
ctx.BeginFrame(dt, width, height);   // clock, mouse, hot-widget recomputation
ctx.ProcessInput(event);             // feed translated host events (optional, per event)
// ... declare your UI with BeginPanel / widgets / DrawText / DrawRect ...
ctx.Render();                        // run layout, flush draw commands to GL
```

- `BeginFrame` rolls the rect registry: rects computed last frame drive input
  and drawing this frame (**one-frame latency** — an egui-style tradeoff that
  keeps layout and hit-testing decoupled).
- `ProcessInput` returns `true` when the UI consumed an event (e.g. a click on
  a widget). Consume policy: the UI blocks events that hit interactive
  widgets, everything else passes through to the host.
- `Render` runs the Yoga layout pass, resolves widget rects, and replays the
  recorded command queue through the batcher.

## Events

The host translates platform input into a `std::variant`:

```cpp
using InputEvent = std::variant<
    MouseMoveEvent,      // Vec2 pos
    MouseButtonEvent,    // button, pressed, pos
    MouseScrollEvent,    // delta, pos
    KeyEvent,            // key code, pressed, repeat
    TextInputEvent>;     // codepoint
```

Mouse coordinates are in the same pixel space the UI is drawn in (y-down,
origin top-left). There is no inversion, no scaling — translate 1:1.

## Retained state

`WidgetStateStore` keeps one `WidgetState` per widget ID (FNV-1a hash of the
label + the ID stack):

```cpp
struct WidgetState {
    float hoverT;      // eased 0..1 hover amount
    float pressT;      // eased 0..1 press amount
    float valueT;      // widget value (e.g. toggle knob position)
};
```

Targets are set by widgets each frame; `Advance()` eases them with a
frame-rate-independent exponential filter (`ExpSmooth`, ~14/s for hover,
~22/s for press). Un-touched states are garbage-collected after 240 frames.

## Widget IDs

IDs are derived from the string label combined with the container ID stack, so
identical labels in different panels never collide. Hashing is deterministic
(FNV-1a 64, tested against published reference vectors).

## Drawing primitives

You rarely need these, but they are the public surface everything else uses:

```cpp
ctx.DrawRect(const Rect& rect, const RectStyle& style);
ctx.DrawText(Vec2 pos, std::string_view utf8, const Color& color,
             float fontSize, const Rect* clipRect = nullptr);
Vec2 ctx.MeasureText(std::string_view utf8, float fontSize) const;
```

`RectStyle` packs fill, per-corner radii, border and a soft drop shadow
(offset / spread / blur / opacity). `DrawText` decodes UTF-8, walks glyph
quads, and clips against `clipRect` (passing your panel content rect is the
convention).

## Command queue

`ctx.Render()` first records a frame's worth of POD commands
(`RoundedRect`, `Glyph`, `ClipPush`/`ClipPop`) into a `ui::CommandQueue`,
then replays them through the OpenGL batcher. The queue is pure data — you can
inspect it headlessly (the test suite does exactly that) or replay it with
your own backend later.

## What the library does NOT do

- No windowing, no threading, no buffer swapping — the host owns the loop.
- No global state, no singletons: one `ui::Context` per process, pass it around.
- No rendering beyond GL 3.3 core today (older variants are roadmap Stage 10).