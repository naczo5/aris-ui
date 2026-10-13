#pragma once
// aris-ui — the central UI context.
//
// Caller-driven execution model: the host owns the event loop, window and GL
// context. Each frame the host calls BeginFrame(), feeds events through
// ProcessInput() (getting back a "consumed" flag so it can block the game),
// builds its panels/widgets, and finally calls Render() to flush the command
// queue through the OpenGL backend.
//
// Interaction uses the previous frame's layout rects (egui-style one-frame
// latency); rendering uses the rects computed this frame.
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "ui/core/math.hpp"
#include "ui/core/easing.hpp"
#include "ui/core/event.hpp"
#include "ui/core/id.hpp"
#include "ui/core/theme.hpp"
#include "ui/core/widget_state.hpp"
#include "ui/layout/layout_node.hpp"
#include "ui/renderer/command_queue.hpp"

namespace ui {

class FontAtlas;
class Batcher;
struct GlStateGuard;

// ---------------------------------------------------------------------------
// Container (panel or nested flex group) — retained across frames
// ---------------------------------------------------------------------------
struct Container {
    std::unique_ptr<LayoutNode> ownedNode;             // panel roots own their node
    LayoutNode* externalNode = nullptr;                // group containers wrap a parent slot node
    std::vector<std::unique_ptr<LayoutNode>> children; // retained widget child nodes
    std::vector<WidgetId> childIds;                    // widget id per child slot
    std::vector<bool> childInTree;                     // whether child is attached to yoga tree
    std::size_t used = 0;                              // children used this frame
    bool isRow = false;
    std::uint64_t lastTouchFrame = 0;

    LayoutNode& Node();   // external node or owned node
    void ResetFrame(bool row);
    // Fetch (or create) the child node for slot `used`, attaching it to the tree.
    LayoutNode& NextChild(WidgetId id);
    // Detach children beyond `used` from the yoga tree.
    void CullUnused();
};

struct PanelStyle {
    Color bg = Color::Transparent();
    Color border = Color::Transparent();
    float borderWidth = 0.f;
    Vec4 cornerRadii = Vec4(0.f);
    Vec2 shadowOffset = Vec2(0.f, 0.f);
    float shadowSpread = 0.f, shadowBlur = 0.f, shadowOpacity = 0.f;
    float backdropBlur = 0.f;   // frosted-glass blur behind the panel (px, 0 = off)
    float padding = 0.f;
    const char* title = nullptr;
    float titleSize = 0.f;

    static PanelStyle FromTheme(const Theme& th, float padding = 16.f, const char* title = nullptr);
};

// Snapshot handed to widgets by Context::BeginWidget.
struct WidgetFrame {
    WidgetId id = kInvalidId;
    Rect rect;                 // previous-frame rect (input/draw position)
    bool hot = false;          // mouse over widget
    bool active = false;       // widget is pressed/dragged
    bool clicked = false;      // press+release over widget this frame
    float dt = 0.f;
    WidgetState* state = nullptr;
    LayoutNode* node = nullptr;
};

// ---------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------
class Context {
public:
    Context();
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    // --- configuration ---
    void SetTheme(const Theme& theme) { theme_ = theme; }
    const Theme& theme() const { return theme_; }
    void SetFont(const FontAtlas* font) { font_ = font; }
    const FontAtlas* font() const { return font_; }

    // --- frame lifecycle (caller-driven) ---
    void BeginFrame(float dt, float width, float height, float scale = 1.f);
    bool ProcessInput(const InputEvent& event);   // returns true if the UI consumed it
    void Render();                                 // layout + flush draw commands (GL)

    // --- containers ---
    void BeginPanel(const char* id, const Rect& rect, const PanelStyle& style = {});
    void EndPanel();
    void BeginRow();
    void BeginColumn();
    void EndGroup();   // closes the innermost row/column group
    void BeginGroup(bool row);

    // --- ID stack ---
    void PushId(WidgetId seed) { idStack_.push_back(HashCombine(idStack_.back(), seed)); }
    void PushId(const char* str) { idStack_.push_back(HashString(idStack_.back(), str)); }
    void PopId() { if (idStack_.size() > 1) idStack_.pop_back(); }
    WidgetId MakeId(const char* label) const { return HashString(idStack_.back(), label); }

    // --- widget plumbing (used by ui::Button/Toggle/Slider/...) ---
    WidgetFrame BeginWidget(const char* label, float height, float minWidth = 0.f);
    void EndWidget();

    // --- drawing ---
    void DrawRect(const Rect& rect, const RectStyle& style);
    // Text. `weight` emboldens (+) or thins (-) glyphs by that many pixels —
    // e.g. 0.35 for a semi-bold feel from a regular font, -0.15 for light.
    void DrawText(Vec2 pos, std::string_view text, const Color& color, float fontSize,
                  const Rect* clipRect = nullptr, float weight = 0.f);
    Vec2 MeasureText(std::string_view text, float fontSize) const;
    // Anti-aliased line with round caps (the primitive vector icons are built from).
    void DrawLine(Vec2 a, Vec2 b, float thickness, const Color& color);
    // Filled / outlined circle helpers (outline when `stroke` > 0).
    void DrawCircle(Vec2 center, float radius, const Color& color, float stroke = 0.f);
    // Frosted glass: gaussian-blurs everything already drawn beneath `rect`
    // (rounded by `radii`) and mixes `tint` (its alpha = mix amount) over it.
    // Draw the translucent surface *after* this call. `blurRadius` in px.
    void DrawBackdropBlur(const Rect& rect, const Vec4& radii, float blurRadius,
                          const Color& tint = Color::Transparent());
    // Scissor clipping (axis-aligned; nests by intersection).
    void PushClip(const Rect& rect) { queue_.PushClip(rect); }
    void PopClip() { queue_.PopClip(); }

    // --- absolute-positioned interactive regions ---
    // For overlay UIs that place things by hand instead of through Yoga:
    // registers `rect` as a hit target (above panels) and returns hover/press
    // state with eased transitions. `label` is hashed with the ID stack.
    WidgetFrame Interact(const char* label, const Rect& rect);

    // --- frame info ---
    float dt() const { return dt_; }
    float scale() const { return scale_; }
    Vec2 size() const { return {width_, height_}; }
    Vec2 mouse() const { return mouse_; }

    // --- input state (public for widget implementations) ---
    bool IsHot(WidgetId id) const { return hotId_ == id; }
    bool IsActive(WidgetId id) const { return activeId_ == id; }
    bool WasClicked(WidgetId id) const { return clickedId_ == id; }

private:
    struct HitEntry { WidgetId id; Rect rect; };

    void RunLayoutAndCollectRects();
    void CollectContainerRects(Container& container, Vec2 basePos);
    WidgetId HitTest(Vec2 pos) const;
    bool HotAt(Vec2 pos) const { return HitTest(pos) != kInvalidId; }
    void GcRetained();

    Theme theme_;
    const FontAtlas* font_ = nullptr;

    // frame state
    float dt_ = 0.f, width_ = 0.f, height_ = 0.f, scale_ = 1.f;
    std::uint64_t frame_ = 0;
    Vec2 mouse_;
    std::vector<WidgetId> idStack_{1, 1};   // seed 1

    // interaction
    WidgetId hotId_ = kInvalidId;           // recomputed per frame in BeginFrame
    WidgetId activeId_ = kInvalidId;        // pressed widget (mouse capture)
    WidgetId pressedId_ = kInvalidId;       // id captured at press time
    WidgetId clickedId_ = kInvalidId;       // click completed this frame
    WidgetId releasedId_ = kInvalidId;      // widget released this frame

    // layout registries
    std::vector<HitEntry> prevRects_;        // previous frame (input + drawing)
    std::vector<HitEntry> curRects_;         // this frame (filled at Render)
    std::vector<HitEntry> overlayRects_;     // Interact() regions, appended on top at Render
    std::unordered_map<WidgetId, Rect> rectById_;   // lookup over prevRects_

    // retained panels
    struct Panel {
        WidgetId id;
        Rect rect;
        PanelStyle style;
        Container root;    // root owns its node
        std::uint64_t lastTouchFrame = 0;
    };
    std::unordered_map<WidgetId, std::unique_ptr<Panel>> panels_;
    std::unordered_map<WidgetId, std::unique_ptr<Container>> groups_;
    std::vector<Container*> containerStack_;   // panel roots + groups
    std::vector<Panel*> activePanels_;         // panels begun this frame (draw order)

    // widget state
    WidgetStateStore states_;

    // command output
    CommandQueue queue_;
    std::unique_ptr<Batcher> batcher_;
};

} // namespace ui