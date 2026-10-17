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

// Persistent per-renderer scratch: reusing the span vectors frame to frame
// avoids re-allocating (and re-growing) every span's vertex buffer on every
// frame, which showed up as per-frame hitch sources in overlay hosts.
struct SpanScratch {
    std::vector<UiSpan> spans;
};

// Fills scratch.spans from `queue`, reusing existing buffers. Spans stay valid
// until the next call.
void BuildUiSpans(const CommandQueue& queue, SpanScratch& scratch);

} // namespace ui
