#include "ui/core/context.hpp"
#include "ui/font/font_atlas.hpp"
#include "ui/renderer/batcher.hpp"
#include "ui/renderer/gl_state_guard.hpp"

#include <yoga/Yoga.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ui {

// ---------------------------------------------------------------------------
// Container
// ---------------------------------------------------------------------------
LayoutNode& Container::Node() {
    return externalNode ? *externalNode : *ownedNode;
}

void Container::ResetFrame(bool row) {
    isRow = row;
    used = 0;
}

LayoutNode& Container::NextChild(WidgetId id) {
    if (used == children.size()) {
        children.push_back(std::make_unique<LayoutNode>());
        childIds.push_back(id);
        childInTree.push_back(false);
    }
    childIds[used] = id;
    LayoutNode& node = *children[used];
    if (!childInTree[used]) {
        YGNodeInsertChild(Node().get(), node.get(), static_cast<unsigned>(used));
        childInTree[used] = true;
    }
    ++used;
    return node;
}

void Container::CullUnused() {
    for (std::size_t i = used; i < children.size(); ++i) {
        if (childInTree[i]) {
            YGNodeRemoveChild(Node().get(), children[i]->get());
            childInTree[i] = false;
        }
    }
}

// ---------------------------------------------------------------------------
// PanelStyle
// ---------------------------------------------------------------------------
PanelStyle PanelStyle::FromTheme(const Theme& th, float padding, const char* title) {
    PanelStyle s;
    s.bg = th.panelBg;
    s.border = th.panelBorder;
    s.borderWidth = th.borderWidth;
    s.cornerRadii = Vec4(th.panelRadius);
    s.shadowOffset = th.shadowOffset;
    s.shadowSpread = 0.f;
    s.shadowBlur = th.shadowBlur;
    s.shadowOpacity = th.shadowOpacity;
    s.backdropBlur = th.panelBlur;
    s.padding = padding;
    s.title = title;
    s.titleSize = th.fontSize + 3.f;
    return s;
}

// ---------------------------------------------------------------------------
// WidgetStateStore
// ---------------------------------------------------------------------------
void WidgetStateStore::Advance(float dt, float hoverSpeed, float pressSpeed, float valueSpeed) {
    for (auto& [id, s] : states_) {
        s.hoverT = ExpSmooth(s.hoverT, s.hoverTarget, dt, hoverSpeed);
        s.pressT = ExpSmooth(s.pressT, s.pressTarget, dt, pressSpeed);
        s.valueT = ExpSmooth(s.valueT, s.valueTarget, dt, valueSpeed);
    }
}

void WidgetStateStore::Gc(std::uint64_t frame, std::uint64_t staleFrames) {
    for (auto it = states_.begin(); it != states_.end();) {
        if (frame - it->second.lastTouchFrame > staleFrames) it = states_.erase(it);
        else ++it;
    }
}

// ---------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------
Context::Context() = default;
Context::~Context() = default;

void Context::BeginFrame(float dt, float width, float height, float scale) {
    dt_ = dt > 0.f ? dt : 1.f / 60.f;
    width_ = width;
    height_ = height;
    scale_ = scale;
    ++frame_;

    // Roll registries: rects computed last frame drive input & drawing this frame.
    prevRects_ = std::move(curRects_);
    curRects_.clear();
    overlayRects_.clear();
    rectById_.clear();
    for (const auto& e : prevRects_) rectById_[e.id] = e.rect;

    // Click/release info was consumed by widgets last frame.
    clickedId_ = kInvalidId;
    releasedId_ = kInvalidId;
    hotId_ = HitTest(mouse_);

    activePanels_.clear();

    // Ease widget transitions toward targets set last frame, and periodically
    // drop stale retained state.
    states_.Advance(dt_);
    if ((frame_ % 300) == 0) {
        states_.Gc(frame_);
        GcRetained();
    }
}

bool Context::ProcessInput(const InputEvent& event) {
    bool consumed = false;

    if (auto* mv = std::get_if<MouseMoveEvent>(&event)) {
        mouse_ = mv->pos;
    } else if (auto* mb = std::get_if<MouseButtonEvent>(&event)) {
        mouse_ = mb->pos;
        if (mb->button == MouseButton::Left) {
            if (mb->pressed) {
                pressedId_ = HitTest(mb->pos);       // mouse capture
                activeId_ = pressedId_;
                consumed = activeId_ != kInvalidId;
            } else {
                releasedId_ = activeId_;
                clickedId_ = (activeId_ != kInvalidId && HitTest(mb->pos) == activeId_)
                                 ? activeId_ : kInvalidId;
                activeId_ = kInvalidId;
                pressedId_ = kInvalidId;
                consumed = releasedId_ != kInvalidId;
            }
        }
    } else if (auto* sc = std::get_if<MouseScrollEvent>(&event)) {
        // Block scroll from reaching the game while over any UI surface.
        consumed = HotAt(sc->pos);
    }
    // KeyEvent / TextInputEvent are not consumed (no text input widgets yet).
    return consumed;
}

void Context::BeginPanel(const char* label, const Rect& rect, const PanelStyle& style) {
    WidgetId id = MakeId(label);
    Panel* panel;
    auto it = panels_.find(id);
    if (it == panels_.end()) {
        panel = panels_.try_emplace(id, std::make_unique<Panel>()).first->second.get();
        panel->root.ownedNode = std::make_unique<LayoutNode>();
    } else {
        panel = it->second.get();
    }
    panel->id = id;
    panel->rect = rect;
    panel->style = style;
    panel->lastTouchFrame = frame_;

    LayoutNode& root = *panel->root.ownedNode;
    root.SetFlexDirection(YGFlexDirectionColumn);
    root.SetAlignItems(YGAlignStretch);
    root.SetWidth(rect.w);
    root.SetHeight(rect.h);
    root.SetGap(YGGutterRow, theme_.rowGap);
    root.SetPadding(YGEdgeLeft, style.padding);
    root.SetPadding(YGEdgeRight, style.padding);
    root.SetPadding(YGEdgeBottom, style.padding);
    root.SetPadding(YGEdgeTop, style.padding);
    if (style.title) {
        const float titleSize = style.titleSize > 0 ? style.titleSize : theme_.fontSize + 3.f;
        root.SetPadding(YGEdgeTop, style.padding + titleSize + 14.f);
    }
    panel->root.ResetFrame(false);

    activePanels_.push_back(panel);
    containerStack_.push_back(&panel->root);
    PushId(id);

    // Draw the panel surface immediately (the rect is caller-provided, so no
    // one-frame latency is needed).
    if (style.backdropBlur > 0.f)
        DrawBackdropBlur(rect, style.cornerRadii, style.backdropBlur, Color(18, 8, 36, 40));
    RectStyle st;
    st.fill = style.bg;
    st.borderColor = style.border;
    st.borderWidth = style.borderWidth;
    st.cornerRadii = style.cornerRadii;
    st.shadowOffset = style.shadowOffset;
    st.shadowSpread = style.shadowSpread;
    st.shadowBlur = style.shadowBlur;
    st.shadowOpacity = style.shadowOpacity;
    DrawRect(rect, st);

    if (style.title) {
        const float titleSize = style.titleSize > 0 ? style.titleSize : theme_.fontSize + 3.f;
        Rect clip(rect.x + style.padding * 0.5f, rect.y + 2.f, rect.w - style.padding, titleSize * 2.5f);
        DrawText(Vec2(rect.x + style.padding, rect.y + style.padding), style.title,
                 theme_.panelTitle, titleSize, &clip);
    }
}

void Context::EndPanel() {
    PopId();
    Container* root = containerStack_.back();
    containerStack_.pop_back();
    root->CullUnused();
}

void Context::BeginRow() { BeginGroup(true); }

void Context::BeginColumn() { BeginGroup(false); }

void Context::BeginGroup(bool row) {
    Container* parent = containerStack_.back();
    // Group identity = label + the slot it occupies in its parent. Tree shape
    // is stable frame-to-frame, so this key is stable too.
    WidgetId id = HashCombine(MakeId(row ? "__row" : "__column"), parent->used);
    auto& g = groups_[id];
    if (!g) g = std::make_unique<Container>();
    g->lastTouchFrame = frame_;
    g->externalNode = &parent->NextChild(id);

    LayoutNode& n = *g->externalNode;
    n.SetFlexDirection(row ? YGFlexDirectionRow : YGFlexDirectionColumn);
    n.SetAlignItems(YGAlignStretch);
    n.SetGap(row ? YGGutterColumn : YGGutterRow, row ? theme_.columnGap : theme_.rowGap);
    if (!row) n.SetFlexGrow(1.f);   // columns split the parent row's width evenly
    g->ResetFrame(row);

    containerStack_.push_back(g.get());
    PushId(id);
}

void Context::EndGroup() {
    PopId();
    Container* g = containerStack_.back();
    containerStack_.pop_back();
    g->CullUnused();
}

WidgetFrame Context::BeginWidget(const char* label, float height, float minWidth) {
    WidgetId id = MakeId(label);
    Container* cont = containerStack_.back();
    LayoutNode& node = cont->NextChild(id);

    // NOTE: width is left UNSET (undefined) so align-stretch fills the parent.
    node.SetHeight(height);
    if (minWidth > 0.f) node.SetMinWidth(minWidth);

    WidgetState& st = states_.Get(id, frame_);
    Rect prev;
    if (auto it = rectById_.find(id); it != rectById_.end()) prev = it->second;
    const bool hot = prev.Valid() && prev.Contains(mouse_);
    const bool active = activeId_ == id;

    // Ease transitions inline (frame-rate independent).
    st.hoverT = ExpSmooth(st.hoverT, hot ? 1.f : 0.f, dt_, 14.f);
    st.pressT = ExpSmooth(st.pressT, active ? 1.f : 0.f, dt_, 22.f);

    WidgetFrame w;
    w.id = id;
    w.rect = prev;
    w.hot = hot;
    w.active = active;
    w.clicked = clickedId_ == id;
    w.dt = dt_;
    w.state = &st;
    w.node = &node;
    return w;
}

void Context::EndWidget() {
    // Reserved for future per-widget teardown (tooltips, focus, etc.).
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void Context::DrawRect(const Rect& rect, const RectStyle& style) {
    queue_.PushRect(rect, style);
}

void Context::DrawLine(Vec2 a, Vec2 b, float thickness, const Color& color) {
    queue_.PushLine(a, b, thickness, color);
}

void Context::DrawTriangle(Vec2 a, Vec2 b, Vec2 c, const Color& color, float stroke) {
    queue_.PushTriangle(a, b, c, color, stroke);
}

void Context::DrawCircle(Vec2 c, float radius, const Color& color, float stroke) {
    RectStyle st;
    st.cornerRadii = Vec4(radius);
    if (stroke > 0.f) {
        st.fill = Color::Transparent();
        st.borderColor = color;
        st.borderWidth = stroke;
    } else {
        st.fill = color;
    }
    queue_.PushRect(Rect(c.x - radius, c.y - radius, radius * 2.f, radius * 2.f), st);
}

void Context::DrawBackdropBlur(const Rect& rect, const Vec4& radii, float blurRadius,
                               const Color& tint) {
    queue_.PushBackdrop(rect, radii, blurRadius, tint);
}

WidgetFrame Context::Interact(const char* label, const Rect& rect) {
    const WidgetId id = MakeId(label);
    overlayRects_.push_back({id, rect});

    WidgetState& st = states_.Get(id, frame_);
    const bool hot = hotId_ == id;
    const bool active = activeId_ == id;
    st.hoverT = ExpSmooth(st.hoverT, hot ? 1.f : 0.f, dt_, 14.f);
    st.pressT = ExpSmooth(st.pressT, active ? 1.f : 0.f, dt_, 22.f);

    WidgetFrame w;
    w.id = id;
    w.rect = rect;
    w.hot = hot;
    w.active = active;
    w.clicked = clickedId_ == id;
    w.dt = dt_;
    w.state = &st;
    return w;
}

void Context::DrawText(Vec2 pos, std::string_view text, const Color& color, float fontSize,
                       const Rect* clipRect, float weight) {
    if (!font_ || text.empty()) return;
    if (fontSize <= 0.f) fontSize = theme_.fontSize;

    float penX = pos.x;
    const float baseline = pos.y + font_->Ascender(fontSize);
    for (size_t i = 0; i < text.size();) {
        // Decode UTF-8 (ASCII fast path; 2–4 byte sequences per RFC 3629).
        std::uint32_t cp = static_cast<unsigned char>(text[i]);
        size_t len = 1;
        if (cp >= 0xF0) { len = 4; cp = cp & 0x07; }
        else if (cp >= 0xE0) { len = 3; cp = cp & 0x0F; }
        else if (cp >= 0xC0) { len = 2; cp = cp & 0x1F; }
        bool complete = (i + len <= text.size());
        for (size_t k = 1; complete && k < len; ++k) {
            const unsigned char cc = static_cast<unsigned char>(text[i + k]);
            if ((cc & 0xC0) != 0x80) { complete = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!complete) break;   // truncated sequence at end of string
        i += len;

        const GlyphInfo* g = font_->GetGlyph(cp);
        if (!g) g = font_->GetGlyph('?');
        if (!g) continue;
        const float gw = g->width * fontSize;
        const float gh = g->height * fontSize;
        if (gw > 0.f && gh > 0.f) {
            Rect quad(penX + g->left * fontSize, baseline - g->top * fontSize, gw, gh);
            bool visible = quad.Valid();
            if (visible && clipRect) {
                visible = quad.x + quad.w > clipRect->x && quad.x < clipRect->Max().x &&
                          quad.y + quad.h > clipRect->y && quad.y < clipRect->Max().y;
            }
            if (visible) {
                const float guv[4] = {g->u0, g->v0, g->u1, g->v1};
                queue_.PushGlyph(quad, guv, color, font_->TextureId(), fontSize, weight);
            }
        }
        penX += g->advance * fontSize;
    }
}

Vec2 Context::MeasureText(std::string_view text, float fontSize) const {
    if (!font_ || text.empty()) return {};
    if (fontSize <= 0.f) fontSize = theme_.fontSize;
    float width = 0.f;
    for (size_t i = 0; i < text.size();) {
        std::uint32_t cp = static_cast<unsigned char>(text[i]);
        size_t len = 1;
        if (cp >= 0xF0) { len = 4; cp = cp & 0x07; }
        else if (cp >= 0xE0) { len = 3; cp = cp & 0x0F; }
        else if (cp >= 0xC0) { len = 2; cp = cp & 0x1F; }
        bool complete = (i + len <= text.size());
        for (size_t k = 1; complete && k < len; ++k) {
            const unsigned char cc = static_cast<unsigned char>(text[i + k]);
            if ((cc & 0xC0) != 0x80) { complete = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!complete) break;
        i += len;
        const GlyphInfo* g = font_->GetGlyph(cp);
        if (!g) g = font_->GetGlyph('?');
        if (g) width += g->advance * fontSize;
    }
    return Vec2(width, font_->LineHeight(fontSize));
}

// ---------------------------------------------------------------------------
// Layout & rendering
// ---------------------------------------------------------------------------
void Context::RunLayoutAndCollectRects() {
    for (Panel* p : activePanels_) {
        YGNodeCalculateLayout(p->root.ownedNode->get(), p->rect.w, p->rect.h, YGDirectionLTR);
        curRects_.push_back({p->id, p->rect});
        CollectContainerRects(p->root, p->rect.Pos());
    }
    // Hand-placed regions are hit-tested above everything laid out by Yoga.
    curRects_.insert(curRects_.end(), overlayRects_.begin(), overlayRects_.end());
}

void Context::CollectContainerRects(Container& container, Vec2 basePos) {
    for (std::size_t i = 0; i < container.used && i < container.children.size(); ++i) {
        const LayoutNode& child = *container.children[i];
        const Rect rect(basePos.x + child.LayoutLeft(), basePos.y + child.LayoutTop(),
                        child.LayoutWidth(), child.LayoutHeight());
        const WidgetId childId = container.childIds[i];
        curRects_.push_back({childId, rect});
        if (auto it = groups_.find(childId); it != groups_.end()) {
            CollectContainerRects(*it->second, rect.Pos());
        }
    }
}

void Context::DiscardFrame() {
    queue_.Reset();
    clickedId_ = kInvalidId;
    releasedId_ = kInvalidId;
}

void Context::ReleaseGraphics() {
    if (backend_) backend_->ReleaseDevice();
    if (batcher_) {
        batcher_->Shutdown();
        batcher_.reset();
    }
}

void Context::Render() {
    RunLayoutAndCollectRects();

    if (std::getenv("ARIS_DEBUG")) {
        static int dbgFrame = 0;
        if (++dbgFrame <= 8) {
            std::size_t rectCmds = 0, glyphCmds = 0;
            for (const auto& c : queue_.Commands()) {
                if (c.type == DrawCommand::Type::RoundedRect) ++rectCmds;
                else if (c.type == DrawCommand::Type::Glyph) ++glyphCmds;
            }
            std::fprintf(stderr, "[aris] frame %d: rectCmds=%zu glyphCmds=%zu prevRects=%zu curRects=%zu\n",
                         dbgFrame, rectCmds, glyphCmds, prevRects_.size(), curRects_.size());
            for (std::size_t i = 0; i < curRects_.size() && i < 6; ++i) {
                std::fprintf(stderr, "[aris]   rect[%zu] id=%llu (%.1f, %.1f, %.1f x %.1f)\n", i,
                             (unsigned long long)curRects_[i].id, curRects_[i].rect.x, curRects_[i].rect.y,
                             curRects_[i].rect.w, curRects_[i].rect.h);
            }
        }
    }

    if (backend_) {
        backend_->Draw(queue_, font_, width_, height_);
    } else {
        if (!batcher_) batcher_ = std::make_unique<Batcher>();
        if (!batcher_->Initialized() && !batcher_->Initialize()) {
            queue_.Reset();
            clickedId_ = kInvalidId;
            releasedId_ = kInvalidId;
            return;
        }
        // Defensive state guard: the host pipeline is untouched by our draw.
        GlStateGuard guard;
        batcher_->Draw(queue_, font_, width_, height_);
    }

    queue_.Reset();
    clickedId_ = kInvalidId;
    releasedId_ = kInvalidId;
}

// ---------------------------------------------------------------------------
// Hit testing & GC
// ---------------------------------------------------------------------------
WidgetId Context::HitTest(Vec2 pos) const {
    // Reverse scan: the last registered rect is drawn last, so it is on top.
    for (auto it = prevRects_.rbegin(); it != prevRects_.rend(); ++it) {
        if (it->rect.Contains(pos) && it->id != kInvalidId) return it->id;
    }
    for (auto it = curRects_.rbegin(); it != curRects_.rend(); ++it) {
        if (it->rect.Contains(pos) && it->id != kInvalidId) return it->id;
    }
    return kInvalidId;
}

void Context::GcRetained() {
    for (auto it = panels_.begin(); it != panels_.end();) {
        if (frame_ - it->second->lastTouchFrame > 300) it = panels_.erase(it);
        else ++it;
    }
    for (auto it = groups_.begin(); it != groups_.end();) {
        if (frame_ - it->second->lastTouchFrame > 300) it = groups_.erase(it);
        else ++it;
    }
}

} // namespace ui