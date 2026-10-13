#pragma once
// aris-ui — OpenGL batcher backend.
// Replays a CommandQueue into one (or few) draw call(s) using a single
// interleaved vertex buffer and an SDF shader pipeline. No GL types leak
// into this header (GL context must be current when Draw is called).
#include <cstdint>
#include <vector>
#include "ui/renderer/command_queue.hpp"

namespace ui {

class FontAtlas;

class Batcher {
public:
    Batcher() = default;
    ~Batcher();
    Batcher(const Batcher&) = delete;
    Batcher& operator=(const Batcher&) = delete;

    // Builds shaders/buffers; requires a current GL context. Idempotent.
    bool Initialize();
    void Shutdown();
    bool Initialized() const { return initialized_; }

    // Renders the command queue. Caller is responsible for wrapping this in
    // a GlStateGuard scope. uiScale multiplies coordinates onto the framebuffer.
    void Draw(const CommandQueue& queue, const FontAtlas* atlas,
              float framebufferWidth, float framebufferHeight);

private:
    struct UiVertex {
        float pos[2];          // unit quad corner (0..1)
        float rect[4];         // content rect (screen px): x, y, w, h
        float radii[4];        // per-corner radii
        std::uint8_t color[4];
        std::uint8_t borderColor[4];
        float params[4];       // borderWidth, shadowSpread, shadowBlur, isGlyph
        float shadow[4];       // offsetX, offsetY, shadowOpacity, pad
        float uv[4];           // glyph uv rect (0 when unused)
    };

    void Flush(std::uint32_t atlasId, float fontSize, float atlasPxRange,
               float atlasGenSize, std::uint32_t atlasTex, float framebufferHeight);
    void ApplyScissor(const Rect* clipStack, int depth, float framebufferHeight);
    void PushQuad(const UiVertex& proto);

    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int whiteTexture_ = 0;

    int uResolution_ = -1;
    int uPxRange_ = -1;
    int uTex_ = -1;

    bool initialized_ = false;
    std::vector<UiVertex> vertices_;
    std::vector<UiVertex> pending_;

    std::uint32_t currentAtlas_ = 0;
    float currentFontSize_ = -1.f;
};

} // namespace ui