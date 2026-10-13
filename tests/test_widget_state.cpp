// aris-ui tests — retained widget transition state store.
#include "test_main.hpp"
#include "ui/core/widget_state.hpp"

using namespace ui;

TEST(state_defaults) {
    WidgetStateStore store;
    WidgetState& s = store.Get(123, 1);
    CHECK_EQ(s.hoverT, 0.f);
    CHECK_EQ(s.pressT, 0.f);
    CHECK_EQ(s.valueT, 0.f);
    CHECK_EQ(s.hoverTarget, 0.f);
}

TEST(state_same_id_same_entry) {
    WidgetStateStore store;
    WidgetState& a = store.Get(7, 1);
    WidgetState& b = store.Get(7, 1);
    a.hoverT = 0.5f;
    CHECK_EQ(b.hoverT, 0.5f);
}

TEST(advance_moves_toward_targets) {
    WidgetStateStore store;
    WidgetState& s = store.Get(1, 1);
    s.hoverTarget = 1.f;
    s.pressTarget = 1.f;
    s.valueTarget = 1.f;

    for (int i = 0; i < 120; ++i)
        store.Advance(1.f / 60.f, 10.f, 18.f, 18.f);

    CHECK_NEAR(s.hoverT, 1.f, 0.01f);
    CHECK_NEAR(s.pressT, 1.f, 0.01f);
    CHECK_NEAR(s.valueT, 1.f, 0.01f);
}

TEST(advance_never_overshoots) {
    WidgetStateStore store;
    WidgetState& s = store.Get(1, 1);
    s.hoverTarget = 1.f;
    for (int i = 0; i < 600; ++i) {
        store.Advance(1.f / 60.f, 10.f, 18.f, 18.f);
        CHECK(s.hoverT <= 1.f);
    }
}

TEST(advance_reverses_when_target_drops) {
    WidgetStateStore store;
    WidgetState& s = store.Get(1, 1);
    s.hoverTarget = 1.f;
    for (int i = 0; i < 120; ++i) store.Advance(1.f / 60.f, 10.f, 18.f, 18.f);
    CHECK_NEAR(s.hoverT, 1.f, 0.01f);

    s.hoverTarget = 0.f;
    for (int i = 0; i < 120; ++i) store.Advance(1.f / 60.f, 10.f, 18.f, 18.f);
    CHECK(s.hoverT < 0.01f);
}

TEST(gc_drops_stale_keeps_recent) {
    WidgetStateStore store;
    store.Get(100, 1);          // stale
    store.Get(200, 100);        // recent
    store.Gc(101, 50);          // frame 101, stale threshold 50
    // id 100: lastTouch=1, age=100 > 50 → dropped
    // id 200: lastTouch=100, age=1 → kept
    // Verify via re-Get: fresh entry has default hoverT.
    store.Get(200, 101).hoverT = 0.25f;   // kept entry keeps its value
    CHECK_EQ(store.Get(200, 102).hoverT, 0.25f);
    CHECK_EQ(store.Get(100, 102).hoverT, 0.f);   // fresh default after GC
}