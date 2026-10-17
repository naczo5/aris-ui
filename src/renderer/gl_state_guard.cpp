#include "ui/renderer/gl_state_guard.hpp"
#include "glad/gl.h"

#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif

#include <cstring>

namespace ui {

// Snapshot of every GL state the UI backend touches (or might corrupt).
struct GlStateGuard::State {
    GLint program = 0;
    GLint vertexArray = 0;
    GLint arrayBuffer = 0;
    GLint elementArrayBuffer = 0;
    GLint drawFramebuffer = 0;
    GLint readFramebuffer = 0;
    GLint renderbuffer = 0;
    GLint activeTexture = GL_TEXTURE0;
    GLint texture0 = 0;

    GLboolean blendEnabled = GL_FALSE;
    GLboolean depthTestEnabled = GL_FALSE;
    GLboolean cullFaceEnabled = GL_FALSE;
    GLboolean scissorTestEnabled = GL_FALSE;
    GLboolean stencilTestEnabled = GL_FALSE;
    GLboolean ditherEnabled = GL_FALSE;
    GLboolean alphaTestEnabled = GL_FALSE;

    GLint blendSrcRGB = 0, blendDstRGB = 0, blendSrcAlpha = 0, blendDstAlpha = 0;
    GLint blendEquationRGB = 0, blendEquationAlpha = 0;
    GLint cullFaceMode = 0;
    GLint depthFunc = 0;
    GLint frontFace = 0;
    GLint scissorBox[4] = {};
    GLint viewport[4] = {};
    GLboolean colorMask[4] = {};
    GLboolean depthMask = GL_TRUE;
    GLint pixelPackBuffer = 0;
    GLint pixelUnpackBuffer = 0;
    GLfloat clearColor[4] = {};
};

GlStateGuard::GlStateGuard() : state_(new State()) {
    State& s = *state_;

    glGetIntegerv(GL_CURRENT_PROGRAM, &s.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &s.vertexArray);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s.arrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &s.elementArrayBuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s.drawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s.readFramebuffer);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &s.renderbuffer);

    glGetIntegerv(GL_ACTIVE_TEXTURE, &s.activeTexture);
    // The UI only binds unit 0. Walking every unit stalls the GPU each frame.
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &s.texture0);
    glActiveTexture(s.activeTexture);

    s.blendEnabled = glIsEnabled(GL_BLEND);
    s.depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    s.cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    s.scissorTestEnabled = glIsEnabled(GL_SCISSOR_TEST);
    s.stencilTestEnabled = glIsEnabled(GL_STENCIL_TEST);
    s.ditherEnabled = glIsEnabled(GL_DITHER);
    s.alphaTestEnabled = glIsEnabled(GL_ALPHA_TEST);

    glGetIntegerv(GL_BLEND_SRC_RGB, &s.blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &s.blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &s.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &s.blendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &s.blendEquationRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &s.blendEquationAlpha);
    glGetIntegerv(GL_CULL_FACE_MODE, &s.cullFaceMode);
    glGetIntegerv(GL_DEPTH_FUNC, &s.depthFunc);
    glGetIntegerv(GL_FRONT_FACE, &s.frontFace);
    glGetIntegerv(GL_SCISSOR_BOX, s.scissorBox);
    glGetIntegerv(GL_VIEWPORT, s.viewport);
    glGetBooleanv(GL_COLOR_WRITEMASK, s.colorMask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &s.depthMask);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &s.pixelPackBuffer);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &s.pixelUnpackBuffer);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, s.clearColor);
}

GlStateGuard::~GlStateGuard() {
    State& s = *state_;

    if (s.blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (s.depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (s.cullFaceEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (s.scissorTestEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (s.stencilTestEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
    if (s.ditherEnabled) glEnable(GL_DITHER); else glDisable(GL_DITHER);
    if (s.alphaTestEnabled) glEnable(GL_ALPHA_TEST); else glDisable(GL_ALPHA_TEST);

    glBlendFuncSeparate(s.blendSrcRGB, s.blendDstRGB, s.blendSrcAlpha, s.blendDstAlpha);
    glBlendEquationSeparate(s.blendEquationRGB, s.blendEquationAlpha);
    glCullFace(s.cullFaceMode);
    glDepthFunc(s.depthFunc);
    glFrontFace(s.frontFace);
    glScissor(s.scissorBox[0], s.scissorBox[1], s.scissorBox[2], s.scissorBox[3]);
    glViewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]);
    glColorMask(s.colorMask[0], s.colorMask[1], s.colorMask[2], s.colorMask[3]);
    glDepthMask(s.depthMask);
    glClearColor(s.clearColor[0], s.clearColor[1], s.clearColor[2], s.clearColor[3]);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, s.pixelPackBuffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, s.pixelUnpackBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, s.renderbuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s.readFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.drawFramebuffer);

    glBindBuffer(GL_ARRAY_BUFFER, s.arrayBuffer);
    // ELEMENT_ARRAY_BUFFER binding is VAO state — restore only with the VAO bound.
    if (s.vertexArray != 0) {
        glBindVertexArray(s.vertexArray);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.elementArrayBuffer);
    } else {
        glBindVertexArray(0);
    }
    glUseProgram(s.program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s.texture0);
    glActiveTexture(s.activeTexture);

    delete state_;
    state_ = nullptr;
}

} // namespace ui