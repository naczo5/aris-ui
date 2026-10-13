#pragma once
// aris-ui — generic input events the host forwards to the UI
#include <cstdint>
#include <variant>
#include "ui/core/math.hpp"

namespace ui {

enum class MouseButton : std::uint8_t { Left, Right, Middle, Back, Forward };

struct MouseMoveEvent {
    Vec2 pos;
};

struct MouseButtonEvent {
    MouseButton button = MouseButton::Left;
    bool pressed = false;
    Vec2 pos;
};

struct MouseScrollEvent {
    Vec2 delta;   // x: horizontal, y: vertical (positive = scroll up)
    Vec2 pos;
};

struct KeyEvent {
    std::int32_t key = 0;   // platform-agnostic key code
    bool pressed = false;
    bool repeat = false;
};

struct TextInputEvent {
    std::uint32_t codepoint = 0;
};

using InputEvent = std::variant<MouseMoveEvent, MouseButtonEvent, MouseScrollEvent, KeyEvent, TextInputEvent>;

} // namespace ui