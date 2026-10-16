#pragma once
#include <cstdint>
#include <vector>
#include "ui/renderer/command_queue.hpp"
#include "ui/renderer/ui_vertex.hpp"

namespace ui {

// One run of geometry that shares a scissor and a glyph batch key,
// or a single backdrop command (OpenGL only).
struct UiSpan {
    bool backdrop = false;
    DrawCommand backdropCmd;
    std::uint32_t atlasId = 0;
    float fontSize = -1.f;
    bool scissor = false;
    float scissorX = 0.f, scissorY = 0.f, scissorW = 0.f, scissorH = 0.f;
    std::vector<UiVertex> verts;
};

void BuildUiSpans(const CommandQueue& queue, std::vector<UiSpan>& spans);

} // namespace ui
