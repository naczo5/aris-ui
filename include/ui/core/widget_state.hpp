#pragma once
// aris-ui — per-widget retained transition state
#include <cstdint>
#include <unordered_map>
#include "ui/core/id.hpp"

namespace ui {

// Retained per-widget transition state (hover offsets, active scales,
// color fades, widget-specific animated values). Updated every frame by the
// built-in easing engine.
struct WidgetState {
    float hoverT = 0.f;      // eased [0..1] hover amount
    float pressT = 0.f;      // eased [0..1] press/active amount
    float valueT = 0.f;      // widget-specific animated value (e.g. toggle knob)

    // targets (set by widgets each frame, advanced by the state store)
    float hoverTarget = 0.f;
    float pressTarget = 0.f;
    float valueTarget = 0.f;

    std::uint64_t lastTouchFrame = 0;
};

class WidgetStateStore {
public:
    WidgetState& Get(WidgetId id, std::uint64_t frame) {
        WidgetState& s = states_[id];
        s.lastTouchFrame = frame;
        return s;
    }

    // Advance all eased values toward their targets.
    void Advance(float dt, float hoverSpeed = 14.f, float pressSpeed = 22.f, float valueSpeed = 16.f);

    // Drop states for widgets not touched for `staleFrames` frames.
    void Gc(std::uint64_t frame, std::uint64_t staleFrames = 240);

    void Clear() { states_.clear(); }

private:
    std::unordered_map<WidgetId, WidgetState> states_;
};

} // namespace ui