# Layout

Layout is powered by a vendored Yoga (flexbox) tree, exposed through an RAII
wrapper: `ui::LayoutNode`. You usually interact with it through panels and
groups rather than nodes directly.

## Panels

A panel is an absolutely positioned root node with a title, padding, border
and shadow. Panels are the *containers* of the retained layout tree — they are
identified by string id and reused across frames.

```cpp
ui::PanelStyle ps = ui::PanelStyle::FromTheme(ui::Theme::Default(), 18.f, "Title");
ctx.BeginPanel("main", ui::Rect(28, 28, 470, 640), ps);
// ... declare rows, columns, widgets ...
ctx.EndPanel();
```

- `Rect` is the on-screen position/size in pixels (y-down).
- `PanelStyle::FromTheme(theme, padding, title)` derives the standard look;
  every field is overridable (colors, radii, shadow, title size).
- `title` is optional; a `nullptr` title renders a borderless content panel.

## Rows and columns

```cpp
ctx.BeginRow();          // horizontal flex container
ctx.BeginColumn();       // vertical flex container inside a row
// ... widgets stack inside ...
ctx.EndGroup();          // closes the innermost open group
ctx.EndGroup();
```

`BeginRow()`/`BeginColumn()` are thin wrappers over `BeginGroup(bool row)`.
Groups use `rowGap`/`columnGap` from the theme and stretch children by
default. The demo runner builds a two-column panel exactly this way — see
`examples/common/AppSupport.hpp::BuildDemoUi`.

## Styling nodes directly

For custom layout behavior, use `ui::LayoutNode` (this is what the internals
use):

```cpp
ui::LayoutNode node;
node.SetFlexDirection(YGFlexDirectionRow);
node.SetGap(YGGutterColumn, 12.f);
node.SetFlexGrow(1.f);
node.SetPaddingAll(16.f);
node.SetWidth(400.f);
```

Computed rects read back with `LayoutLeft/Top/Width/Height()` (parent-relative)
or `ChildRect(parent)` for the absolute rect. The full Yoga style surface is
wrapped: flex, grow/shrink, align, gaps, padding/margin/border per edge,
min/max constraints, position type.

## One-frame latency

Widget rects come from the *previous* frame's layout. This is deliberate:

- input hit-testing never depends on work done later in the same frame;
- widgets can draw immediately with stable geometry;
- the cost is a single frame of delay when a panel resizes or first appears.

If you need a new panel to appear already laid out, declare it one frame
before making it visible (or accept the one-frame pop — it's imperceptible at
overlay frame rates).