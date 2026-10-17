#pragma once
// Host-owned rasterizer. The default OpenGL batcher is used when no backend is set.
#include "ui/renderer/command_queue.hpp"

namespace ui {

class FontAtlas;

class DrawBackend {
public:
    virtual ~DrawBackend() = default;
    // Records (and on GL, executes) `queue` for a width x height framebuffer.
    // The queue is pure data; `atlas` may be null. Some backends need the atlas
    // uploaded outside the draw pass, which they expose as an extra step the
    // host calls once per frame (see VulkanRenderer::Prepare).
    virtual void Draw(const CommandQueue& queue, const FontAtlas* atlas,
                      float width, float height) = 0;
    // Drop GPU objects after a device or GL context loss. CPU state stays.
    virtual void ReleaseDevice() {}
};

} // namespace ui
