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

The command queue is pure data (see `include/ui/renderer/command_queue.hpp`).
`Context::SetDrawBackend()` sends `Render()` at a `DrawBackend`. With no
backend set, the OpenGL batcher runs inside `GlStateGuard`.

`FontAtlas::Load` keeps the atlas on the CPU (`Pixels()`, `Revision()`).
Each backend uploads. `TextureId()` is a stable batch key, not a GL name.

`DrawCircle` is a rounded rect. `DrawTriangle` is its own command (filled,
or stroked when `stroke` > 0). Lines and triangles share the quad SDF path
(`params.w` 2 and 3).

`VulkanRenderer` records into a command buffer the host already put inside
its render pass. Call `Prepare(atlas)` before the pass so font uploads are
not issued mid-pass. Backdrop blur is skipped on Vulkan.

Fill in `VulkanCreateInfo` fully: `renderPass`, `subpass`, `sampleCount` must
describe the pass the UI records in, and `framesInFlight` must match how many
frames the host keeps in flight (the renderer rotates its vertex buffer and
descriptor set across that many slots). Recreating the render pass — a
swapchain resize — invalidates the pipeline: call `Init()` again. `Prepare`
and `ReleaseDevice` wait for the device because they replace images.

`Context::DiscardFrame()` drops the recorded commands without drawing (the
host decided not to present this frame); `Context::ReleaseGraphics()` drops the
GPU objects of the active backend, for when the GL context or Vulkan device
goes away. The CPU-side atlas survives both, so the next frame re-uploads.

The shared vertex is `ui::UiVertex` (120 bytes). OpenGL tries `#version 330 core`,
then `330`, then `150`, so a compatibility context can still compile the shader.