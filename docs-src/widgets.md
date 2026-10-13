# Widgets

All widgets are free functions taking the context first. They require an
active panel (`BeginPanel`/`EndPanel`) because layout belongs to the Yoga
tree. Every widget draws *and* handles input in the same call.

```cpp
#include "ui/widgets/widgets.hpp"
```

## API

| Function | Signature | Behavior |
| --- | --- | --- |
| Button | `bool Button(Context&, const char* label)` | Returns true on the frame the click completes (press + release inside). |
| Toggle | `void Toggle(Context&, const char* label, bool* value)` | Flips `*value` when clicked; knob position eases via `valueT`. |
| Slider | `void Slider(Context&, const char* label, float* value, float minValue, float maxValue)` | Updates `*value` while dragged; shows a right-aligned value label. |
| Label | `void Label(Context&, const char* text, Color color = {}, float fontSize = 0.f)` | Static text; `fontSize = 0` uses the theme default. |

## Visual states

Colors and shadows ease through `WidgetStateStore` — buttons lift on hover
(`hoverT`) and settle when pressed (`pressT`), toggles slide their knob with
`valueT`. The states are frame-rate independent (exponential smoothing), so
animations look identical at 60 or 240 fps.

| State | Fill | Shadow |
| --- | --- | --- |
| idle | `accent` | opacity 0.18, offset (0,2) |
| hover | `accentHover` | opacity 0.32, offset (0,5) |
| press | `accentActive` | opacity 0.08, offset (0,2) |

You can inspect a widget's eased state yourself via
`ctx.theme()` + `WidgetStateStore` if you build custom controls.

## Custom widgets

Custom controls are just immediate-mode code using the primitives:

```cpp
void ColorDot(ui::Context& ctx, ui::Color color) {
    ui::WidgetFrame w = ctx.BeginWidget("dot", 20.f);
    if (w.rect.Valid()) {
        ui::RectStyle st;
        st.fill = color;
        st.cornerRadii = ui::Vec4(10.f);
        ctx.DrawRect(w.rect, st);
    }
    ctx.EndWidget();
}
```

`BeginWidget(label, height)` creates a full-width flexbox row of the given
height inside the current panel and returns a frame with `rect` (valid from
the previous frame's layout) and `state` (the retained `WidgetState`).

## Theming

Everything visual is a token in `ui::Theme` (see `include/ui/core/theme.hpp`).
Set a custom theme with `ctx.SetTheme(theme)` before the first frame.

| Token | Default | Purpose |
| --- | --- | --- |
| `panelBg` | `0x15191F` | Panel surface |
| `panelBorder` | `0x2A3038` | Panel hairline |
| `text` / `textDim` | `0xE6EDF3` / `0x8B949E` | Primary / secondary text |
| `accent` | `0x7FA8CC` | Interactive fill (buttons, active toggle, slider fill) |
| `accentHover` / `accentActive` | `0x94BBD9` / `0x6A92B7` | Hover / pressed accent |
| `controlBg`, `controlBorder`, `trackBg` | dark greys | Inactive control surfaces |
| `panelRadius` / `controlRadius` | 12 / 8 px | Corner radii |
| `shadowOffset` / `shadowBlur` / `shadowOpacity` | (0,6) / 16 / 0.28 | Panel shadow |
| `fontSize` / `rowGap` / `columnGap` / `controlHeight` | 15 / 10 / 12 / 34 px | Spacing metrics |

Previews of every state live in the repo: run `make preview` and read
`previews/buttons.png`, `previews/buttons-hover.png`, `previews/buttons-press.png`,
`previews/toggles.png`, `previews/sliders.png`.