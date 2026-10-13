#pragma once
// aris-ui — built-in widget set (immediate-friendly control over retained
// state). All functions require an active panel (BeginPanel/EndPanel).
#include "ui/core/context.hpp"

namespace ui {

// Returns true on the frame the button was clicked (press + release inside).
bool Button(Context& ctx, const char* label);

// Renders a toggle switch; flips *value when clicked.
void Toggle(Context& ctx, const char* label, bool* value);

// Renders a horizontal slider; updates *value while dragged.
void Slider(Context& ctx, const char* label, float* value,
            float minValue, float maxValue);

// Simple text label.
void Label(Context& ctx, const char* text, const Color& color = Color::Transparent(),
           float fontSize = 0.f);

} // namespace ui