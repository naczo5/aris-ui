# Rendering

The renderer draws everything as signed-distance fields in a single GL 3.3
core pipeline. No textures for shapes, no geometry for corners — a quad per
widget and a fragment shader that does the work.

## Pipeline per frame

1. Widgets record POD commands into `ui::CommandQueue`
   (`RoundedRect`, `Glyph`, `ClipPush`/`ClipPop`).
2. `Batcher` expands each command into padded quads (96-byte interleaved
   vertices) and batches: one draw for all rects, one draw per atlas/font-size
   run for glyphs.
3. `GlStateGuard` (RAII) saved the host's state before the pass and restores
   it afterwards — program, VAO/VBO/IBO bindings, blend func + equation,
   cull/depth/scissor, viewport, texture bindings, active texture, pixel
   store.

## Blend contract

The batcher **owns** blending for the UI pass and enables premultiplied
alpha:

```glsl
glEnable(GL_BLEND);
glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
```

Every fragment the shaders emit is premultiplied (shadow is pure black with
alpha; fill/border/text are `rgb * a`). Host code that wants its state back
gets exactly what it had.

## Shape shader

Rounded rects are analytic SDFs with per-corner radii:

- antialiased fill via `fwidth`-scaled `smoothstep`
- inner border of configurable width and color
- soft drop shadow: the shadow SDF is the same rect offset by
  `shadowOffset`, grown by `shadowSpread`, with a `±shadowBlur` falloff,
  multiplied by `shadowOpacity`

## Text shader

Glyphs are MSDF (multi-channel signed distance fields) sampled from a single
atlas texture:

```glsl
float sd = median3(sample.rgb) - 0.5;
float screenPxRange = /* atlas pxRange scaled to the current font size */;
float alpha = clamp(sd * screenPxRange + 0.5, 0.0, 1.0);
```

`screenPxRange` is derived per-fragment from `fwidth(uv)` so text stays crisp
at any scale without regenerating anything.

## Clipping

`ClipPush`/`ClipPop` commands map to `glScissor` in y-down pixel coordinates.
The queue tracks depth (max 32) and pairs pushes/pops; nested panels and
text clipping compose naturally.

## Notes for custom backends

The command queue is pure data (see `include/ui/renderer/command_queue.hpp`)
— replay it with anything. The GL batcher is a reference backend. The vertex
format packs: rect (4f), radii (4f), color (4 x u8), border color (4 x u8),
params (4f: border width, shadow spread, shadow blur, is-glyph), shadow
(4f: offset xy, opacity, quad padding), uv (4f), position (2f).