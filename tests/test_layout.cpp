// aris-ui tests — Yoga flexbox layout via the RAII LayoutNode wrapper.
#include "test_main.hpp"
#include "ui/layout/layout_node.hpp"

using namespace ui;

TEST(two_columns_fill_row) {
    // Root row 400x100 with two children of 150 and flex-grow → split rest.
    LayoutNode root;
    root.SetFlexDirection(YGFlexDirectionRow);
    root.SetWidth(400.f);
    root.SetHeight(100.f);

    LayoutNode left, right;
    left.SetWidth(150.f);
    right.SetFlexGrow(1.f);
    YGNodeInsertChild(root.get(), left.get(), 0);
    YGNodeInsertChild(root.get(), right.get(), 1);

    YGNodeCalculateLayout(root.get(), YGUndefined, YGUndefined, YGDirectionLTR);

    CHECK_NEAR(left.LayoutWidth(), 150.f, 1e-3);
    CHECK_NEAR(right.LayoutWidth(), 250.f, 1e-3);   // 400 - 150
    CHECK_NEAR(right.LayoutHeight(), 100.f, 1e-3);  // stretch default
    CHECK_NEAR(left.LayoutTop(), 0.f, 1e-3);
    CHECK_NEAR(right.LayoutLeft(), 150.f, 1e-3);
}

TEST(gap_separates_children) {
    LayoutNode root;
    root.SetFlexDirection(YGFlexDirectionRow);
    root.SetGap(YGGutterColumn, 10.f);
    root.SetWidth(300.f);
    root.SetHeight(50.f);

    LayoutNode a, b;
    a.SetFlexGrow(1.f);
    b.SetFlexGrow(1.f);
    YGNodeInsertChild(root.get(), a.get(), 0);
    YGNodeInsertChild(root.get(), b.get(), 1);

    YGNodeCalculateLayout(root.get(), YGUndefined, YGUndefined, YGDirectionLTR);

    CHECK_NEAR(a.LayoutWidth(), 145.f, 1e-3);       // (300-10)/2
    CHECK_NEAR(b.LayoutLeft(), 155.f, 1e-3);        // 145 + 10 gap
}

TEST(column_children_stretch_and_stack) {
    LayoutNode root;
    root.SetFlexDirection(YGFlexDirectionColumn);
    root.SetWidth(100.f);
    root.SetHeight(200.f);
    root.SetPaddingAll(10.f);

    LayoutNode a, b;
    a.SetHeight(30.f);
    b.SetFlexGrow(1.f);
    YGNodeInsertChild(root.get(), a.get(), 0);
    YGNodeInsertChild(root.get(), b.get(), 1);

    YGNodeCalculateLayout(root.get(), YGUndefined, YGUndefined, YGDirectionLTR);

    CHECK_NEAR(a.LayoutTop(), 10.f, 1e-3);          // padding top
    CHECK_NEAR(a.LayoutHeight(), 30.f, 1e-3);
    CHECK_NEAR(b.LayoutTop(), 40.f, 1e-3);
    CHECK_NEAR(b.LayoutHeight(), 150.f, 1e-3);      // 180 inner - 30
    CHECK_NEAR(b.LayoutWidth(), 80.f, 1e-3);        // stretched, padding sides
}

TEST(child_rect_maps_to_absolute) {
    LayoutNode root;
    root.SetFlexDirection(YGFlexDirectionRow);
    root.SetWidth(200.f);
    root.SetHeight(100.f);
    root.SetPaddingAll(20.f);

    LayoutNode child;
    child.SetFlexGrow(1.f);
    YGNodeInsertChild(root.get(), child.get(), 0);

    YGNodeCalculateLayout(root.get(), YGUndefined, YGUndefined, YGDirectionLTR);

    // Layout values are parent-relative; ChildRect adds the parent origin.
    Rect r = child.ChildRect(root);
    CHECK_NEAR(r.x, 20.f, 1e-3);    // root LayoutLeft=0 + child local 20
    CHECK_NEAR(r.y, 20.f, 1e-3);
    CHECK_NEAR(r.w, 160.f, 1e-3);
    CHECK_NEAR(r.h, 60.f, 1e-3);
}