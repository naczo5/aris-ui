#pragma once
// aris-ui — OpenGL batcher backend.
// Replays a CommandQueue into one (or few) draw call(s) using a single
// interleaved vertex buffer and an SDF shader pipeline. No GL types leak
// into this header (GL context must be current when Draw is called).
#include <cstdint>
#include <vector>
#include "ui/renderer/command_queue.hpp"
#include "ui/renderer/ui_vertex.hpp"

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
    void Flush(std::uint32_t atlasId, float fontSize, float atlasPxRange,
               float atlasGenSize, std::uint32_t atlasTex, float framebufferHeight);
    void ApplyScissor(const Rect* clipStack, int depth, float framebufferHeight);
    void PushQuad(const UiVertex& proto);
    std::uint32_t UploadAtlas(const FontAtlas* atlas);

    // --- backdrop blur (frosted glass) ---
    bool EnsureBackdropPipeline();
    void EnsureBlurTextures(int w, int h);
    void DrawBackdrop(const DrawCommand& cmd, const Rect* clipStack, int clipDepth,
                      float framebufferWidth, float framebufferHeight);

    unsigned int blurBlurProgram_ = 0;   // separable gaussian
    unsigned int blurCompProgram_ = 0;   // masked composite into the host framebuffer
    unsigned int blurVao_ = 0;           // attribute-less VAO for gl_VertexID quads
    unsigned int blurFbo_[2] = {0, 0};
    unsigned int blurTex_[3] = {0, 0, 0};   // 0: full-res capture, 1/2: half-res ping-pong
    int capW_ = 0, capH_ = 0;               // capacity of blurTex_[0]
    bool blurReady_ = false, blurFailed_ = false;

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

    unsigned int atlasTexture_ = 0;
    const FontAtlas* atlasPtr_ = nullptr;
    std::uint64_t atlasRevision_ = 0;
};

} // namespace ui