# Fonts

Text is rendered from offline-generated MSDF atlases. At runtime you load a
PNG + JSON pair — no font parsing, no FreeType dependency in the shipped app.

## Generating an atlas

From the CLI (built as `build/atlasgen`):

```sh
./build/atlasgen /usr/share/fonts/TTF/DejaVuSans.ttf atlas/ui
```

Or from code:

```cpp
ui_atlas::Params params;
params.fontSize = 48.f;         // reference px size (em-normalization basis)
params.pxRange  = 4.f;          // MSDF distance range in atlas pixels
params.firstCodepoint = 32;     // default: printable ASCII
params.lastCodepoint  = 126;
params.extraChars = "\xC3\x97\xE2\x80\x94";   // UTF-8 extras (×, —)

std::string err;
ui_atlas::GenerateAndWrite("font.ttf", "atlas/ui", params, err);
```

`GenerateAndWrite` caches: outputs are regenerated only when the PNG is older
than the font file. Delete the pair after changing generator code.

## Runtime loading

```cpp
ui::FontAtlas font;
if (!font.Load("atlas/ui.png", "atlas/ui.json")) { /* handle error */ }
ctx.SetFont(&font);
```

`Load` uploads the texture (needs a live GL context) and parses the metadata.
Metrics are em-normalized: glyph quads scale by `fontSize` at draw time, so
one atlas serves every size and DPI crisply.

> The atlas owns a GL texture. Call `font.Unload()` (or let it go out of
> scope) **before** your GL context is destroyed.

## JSON schema

```json
{
  "atlas":   { "width": 512, "height": 512 },
  "pxRange": 4,
  "metrics": { "fontSize": 48, "lineHeight": 1.17, "ascender": 0.93, "descender": -0.24 },
  "glyphs": [
    {
      "codepoint": 65,
      "advance": 0.684,
      "planeBounds": [0.008, 0.0, 0.676, 0.729],   // em units, y-up: l, b, r, t
      "atlasBounds": [179, 217, 32.06, 34.99]      // PNG pixels: x, y (top-left origin), w, h
    }
  ]
}
```

- `planeBounds` — glyph ink box in em units (multiplied by `fontSize` to get
  screen-space quads).
- `atlasBounds` — the ink rectangle in atlas pixels. The runtime maps
  `planeBounds` 1:1 onto this rect, so padding never distorts glyphs.
- `metrics` — em-normalized plane metrics; `Ascender(fontSize)` and
  `LineHeight(fontSize)` multiply them out.

## MSDF details (msdfgen v1.12 semantics)

The generator calls msdfgen with these invariants — keep them if you touch it:

- `loadGlyph(..., FONT_SCALING_EM_NORMALIZED)` → shape space is em.
- Projection is `pixel = scale * (shape + translate)` with translate in
  **shape units**: `translate = margin/fs - bounds.{l,b}`.
- `Range` is in the same units as the shape: `Range(pxRange / fs)`.
- `generateMSDF` applies error correction internally; no second pass needed.
- Glyphs are shelf-packed with `pxRange + 1` px separation, blitted after all
  placements so the row stride matches the final atlas size.

## Text layout

`ctx.DrawText` decodes UTF-8, resolves each codepoint through
`FontAtlas::GetGlyph` (falling back to `?`), advances the pen by
`advance * fontSize`, and positions quads at
`baseline - top * fontSize`. `ctx.MeasureText` sums the same advances —
useful for right-aligned labels (see the slider value text).

Shaping and kerning are not implemented yet (roadmap Stage 13).