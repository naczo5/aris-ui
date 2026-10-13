#pragma once
// aris-ui — RAII wrapper over the vendored Yoga flexbox engine
#include <yoga/Yoga.h>
#include "ui/core/math.hpp"

namespace ui {

// Owns a YGNode. Styles are applied through small setters; computed layout
// is read back relative to a parent's absolute position.
class LayoutNode {
public:
    LayoutNode() : node_(YGNodeNew()) {}
    ~LayoutNode() { if (node_) YGNodeFree(node_); }

    LayoutNode(const LayoutNode&) = delete;
    LayoutNode& operator=(const LayoutNode&) = delete;

    YGNodeRef get() const { return node_; }

    // --- container styles ---
    void SetFlexDirection(YGFlexDirection d) { YGNodeStyleSetFlexDirection(node_, d); }
    void SetJustifyContent(YGJustify j) { YGNodeStyleSetJustifyContent(node_, j); }
    void SetAlignItems(YGAlign a) { YGNodeStyleSetAlignItems(node_, a); }
    void SetFlexWrap(YGWrap w) { YGNodeStyleSetFlexWrap(node_, w); }
    void SetGap(YGGutter gutter, float gap) { YGNodeStyleSetGap(node_, gutter, gap); }

    // --- self styles ---
    void SetFlex(float f) { YGNodeStyleSetFlex(node_, f); }
    void SetFlexGrow(float g) { YGNodeStyleSetFlexGrow(node_, g); }
    void SetFlexShrink(float s) { YGNodeStyleSetFlexShrink(node_, s); }
    void SetAlignSelf(YGAlign a) { YGNodeStyleSetAlignSelf(node_, a); }

    void SetWidth(float w) { YGNodeStyleSetWidth(node_, w); }
    void SetHeight(float h) { YGNodeStyleSetHeight(node_, h); }
    void SetAutoWidth() { YGNodeStyleSetWidthAuto(node_); }
    void SetAutoHeight() { YGNodeStyleSetHeightAuto(node_); }
    void SetMinHeight(float h) { YGNodeStyleSetMinHeight(node_, h); }
    void SetMinWidth(float w) { YGNodeStyleSetMinWidth(node_, w); }
    void SetMaxHeight(float h) { YGNodeStyleSetMaxHeight(node_, h); }

    void SetPadding(YGEdge edge, float v) { YGNodeStyleSetPadding(node_, edge, v); }
    void SetPaddingAll(float v) {
        SetPadding(YGEdgeAll, v);
    }
    void SetMargin(YGEdge edge, float v) { YGNodeStyleSetMargin(node_, edge, v); }
    void SetMarginAll(float v) { SetMargin(YGEdgeAll, v); }
    void SetBorder(YGEdge edge, float v) { YGNodeStyleSetBorder(node_, edge, v); }
    void SetPositionType(YGPositionType t) { YGNodeStyleSetPositionType(node_, t); }

    // Computed layout (after YGNodeCalculateLayout on the root).
    float LayoutLeft() const { return YGNodeLayoutGetLeft(node_); }
    float LayoutTop() const { return YGNodeLayoutGetTop(node_); }
    float LayoutWidth() const { return YGNodeLayoutGetWidth(node_); }
    float LayoutHeight() const { return YGNodeLayoutGetHeight(node_); }

    // Absolute rect of this node given the absolute rect of its parent.
    Rect ChildRect(const LayoutNode& parent) const {
        return Rect(parent.LayoutLeft() + LayoutLeft(),
                    parent.LayoutTop() + LayoutTop(),
                    LayoutWidth(), LayoutHeight());
    }

private:
    YGNodeRef node_;
};

} // namespace ui