#pragma once
// aris-ui — defensive OpenGL state guard.
// RAII snapshot/restore of every GL capability the UI backend touches so the
// host application's pipeline is never corrupted (essential when rendering
// inside a hooked game context).

namespace ui {

class GlStateGuard {
public:
    GlStateGuard();   // captures current GL state (context must be current)
    ~GlStateGuard();  // restores the captured state

    GlStateGuard(const GlStateGuard&) = delete;
    GlStateGuard& operator=(const GlStateGuard&) = delete;

private:
    struct State;
    State* state_;
};

} // namespace ui