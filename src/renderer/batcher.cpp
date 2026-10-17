#include "ui/renderer/batcher.hpp"
#include "emit_draw.hpp"
#include "shader_source.hpp"
#include "ui/font/font_atlas.hpp"
#include "ui/renderer/gl_state_guard.hpp"
#include "glad/gl.h"

#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace ui {

// ---------------------------------------------------------------------------
// Shaders — #version 330 core (GL 3.3). Legacy 150-core / 2.1 variants are a
// future stage; the source is kept in one place to make variants easy.
// ---------------------------------------------------------------------------
namespace shaders {

static const char* kVertexSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;        // unit quad corner (0..1)
layout(location = 1) in vec4 aRect;       // content rect (screen px): x, y, w, h
layout(location = 2) in vec4 aRadii;      // per-corner radii (TL, TR, BR, BL)
layout(location = 3) in vec4 aColor;      // fill / text color (0..1)
layout(location = 4) in vec4 aBorderColor;
layout(location = 5) in vec4 aParams;     // borderWidth, shadowSpread, shadowBlur, isGlyph
layout(location = 6) in vec4 aShadow;     // offsetX, offsetY, shadowOpacity, quadPad
layout(location = 7) in vec4 aUv;         // glyph uv rect (u0, v0, u1, v1)
layout(location = 8) in vec4 aColor2;     // gradient end color
layout(location = 9) in vec4 aShadowColor;
layout(location = 10) in vec4 aExtra;     // gradientDir.xy

uniform vec2 uResolution;

out vec2 vPos01;
out vec2 vLocal;
out vec2 vHalf;
out vec4 vRadii;
out vec4 vColor;
out vec4 vBorderColor;
out vec4 vParams;
out vec4 vShadow;
out vec2 vGlyphUv;
out vec4 vColor2;
out vec4 vShadowColor;
out vec4 vExtra;

void main() {
    vPos01 = aPos;
    vec2 size = aRect.zw + 2.0 * vec2(aShadow.w);
    vec2 screen = aRect.xy - vec2(aShadow.w) + aPos * size;
    vLocal = aPos * size - vec2(aShadow.w);   // relative to content rect top-left
    vHalf = aRect.zw * 0.5;
    vRadii = aRadii;
    vColor = aColor;
    vBorderColor = aBorderColor;
    vParams = aParams;
    vShadow = aShadow;
    vColor2 = aColor2;
    vShadowColor = aShadowColor;
    vExtra = aExtra;
    // Interpolate the actual atlas uv so fwidth() is a real per-pixel delta.
    vGlyphUv = vec2(mix(aUv.x, aUv.z, aPos.x), mix(aUv.y, aUv.w, aPos.y));
    vec2 clip = (screen / uResolution) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
}
)GLSL";

static const char* kFragmentSrc = R"GLSL(
#version 330 core
in vec2 vPos01;
in vec2 vLocal;
in vec2 vHalf;
in vec4 vRadii;
in vec4 vColor;
in vec4 vBorderColor;
in vec4 vParams;
in vec4 vShadow;
in vec2 vGlyphUv;
in vec4 vColor2;
in vec4 vShadowColor;
in vec4 vExtra;

uniform sampler2D uTex;
uniform float uPxRange;    // MSDF distance range in screen pixels

out vec4 fragColor;

// Signed distance of a rounded box with per-corner radii (y-down coords).
// radii = (topLeft, topRight, bottomRight, bottomLeft)
float sdRoundRect(vec2 p, vec2 halfSize, vec4 radii) {
    float r = (p.x < 0.0)
        ? ((p.y < 0.0) ? radii.x : radii.w)
        : ((p.y < 0.0) ? radii.y : radii.z);
    vec2 q = abs(p) - halfSize + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float median3(vec3 v) {
    return max(min(v.x, v.y), min(max(v.x, v.y), v.z));
}

// Inigo Quilez signed distance. Negative inside.
float sdTriangle(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 e0 = b - a, e1 = c - b, e2 = a - c;
    vec2 v0 = p - a, v1 = p - b, v2 = p - c;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / max(dot(e0, e0), 1e-6), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / max(dot(e1, e1), 1e-6), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / max(dot(e2, e2), 1e-6), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                     vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

// msdfgen standard: how many atlas pixels correspond to one screen pixel.
float screenPxRange() {
    vec2 texSize = vec2(textureSize(uTex, 0));
    vec2 range = vec2(uPxRange) / texSize;
    vec2 screenTexSize = vec2(1.0) / fwidth(vGlyphUv);
    return max(0.5 * dot(range, screenTexSize), 1.0);
}

void main() {
    // --- text path: multi-channel SDF (isGlyph == 1) ---
    if (vParams.w > 0.5 && vParams.w < 1.5) {
#ifdef ARIS_TEXT_DEBUG_SOLID
        fragColor = vec4(vColor.rgb * vColor.a, vColor.a);
        return;
#endif
        vec3 msdf = texture(uTex, vGlyphUv).rgb;
        float sd = median3(msdf) - 0.5;
        if (sd <= -0.42) discard;
        float screenPxDistance = screenPxRange() * sd;
        // vParams.x = embolden weight in screen px (negative thins the glyph)
        float alpha = clamp(screenPxDistance + 0.5 + vParams.x, 0.0, 1.0);
        float a = vColor.a * alpha;
        if (a < 0.004) discard;
        fragColor = vec4(vColor.rgb * a, a);   // premultiplied
        return;
    }

    // --- triangle path (isGlyph == 3) ---
    // radii.xy / radii.zw / extra.xy are the three points, local to the content rect.
    if (vParams.w > 2.5) {
        float sd = sdTriangle(vLocal, vRadii.xy, vRadii.zw, vExtra.xy);
        float d = (vParams.x > 0.0) ? abs(sd) - vParams.x * 0.5 : sd;
        float alpha = clamp(0.5 - d, 0.0, 1.0);
        float ta = vColor.a * alpha;
        if (ta < 0.004) discard;
        fragColor = vec4(vColor.rgb * ta, ta);
        return;
    }

    // --- line path: round-capped segment (isGlyph == 2) ---
    // radii.xy / radii.zw hold the endpoints relative to the content rect.
    if (vParams.w > 1.5) {
        vec2 a = vRadii.xy, b = vRadii.zw;
        vec2 pa = vLocal - a, ba = b - a;
        float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
        float d = length(pa - ba * h) - vParams.x * 0.5;
        float alpha = clamp(0.5 - d, 0.0, 1.0);
        float la = vColor.a * alpha;
        if (la < 0.004) discard;
        fragColor = vec4(vColor.rgb * la, la);
        return;
    }

    // --- shape path: SDF rounded rect + border + soft drop shadow ---
    vec2 p = vLocal - vHalf;
    float maxR = min(vHalf.x, vHalf.y);
    vec4 radii = clamp(vRadii, vec4(0.0), vec4(maxR));
    float sd = sdRoundRect(p, vHalf, radii);
    float aa = max(fwidth(sd), 1e-4);

    float fill = 1.0 - smoothstep(-aa, aa, sd);

    // Optional linear gradient across the content rect.
    vec4 fillCol = vColor;
    if (dot(vExtra.xy, vExtra.xy) > 0.0) {
        vec2 uv = (vLocal / (2.0 * vHalf)) - 0.5;
        float t = dot(uv, vExtra.xy) / max(abs(vExtra.x) + abs(vExtra.y), 1e-4) + 0.5;
        fillCol = mix(vColor, vColor2, clamp(t, 0.0, 1.0));
    }

    // Border is drawn inside the shape edge (CSS border-box semantics).
    float bw = max(vParams.x, 0.0);
    float borderA = 0.0;
    if (bw > 0.0 && vBorderColor.a > 0.0) {
        borderA = fill * smoothstep(-bw - aa, -bw + aa, sd);
    }

    float shadowA = 0.0;
    if (vShadow.z > 0.0) {
        vec2 sp = p - vShadow.xy;
        float ssd = sdRoundRect(sp, vHalf + vec2(vParams.y), radii);
        float blur = max(vParams.z, 0.001);
        // Gaussian-ish falloff; knocked out under the shape so translucent
        // surfaces don't show their own shadow through themselves.
        float k = clamp(0.5 - ssd / (2.0 * blur), 0.0, 1.0);
        shadowA = (k * k * (3.0 - 2.0 * k)) * vShadow.z * vShadowColor.a * (1.0 - fill);
    }

    // Premultiplied-over compositing: shadow <- fill <- border
    vec4 acc = vec4(vShadowColor.rgb * shadowA, shadowA);
    float fa = fillCol.a * fill;
    acc = vec4(fillCol.rgb * fa, fa) + acc * (1.0 - fa);
    float ba = vBorderColor.a * borderA;
    acc = vec4(vBorderColor.rgb * ba, ba) + acc * (1.0 - ba);

    if (acc.a < 0.004) discard;
    fragColor = acc;
}
)GLSL";

} // namespace shaders

// ---------------------------------------------------------------------------
// GL helpers
// ---------------------------------------------------------------------------
namespace {

GLuint CompileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei len = 0;
        glGetShaderInfoLog(shader, sizeof(log), &len, log);
        std::fprintf(stderr, "[aris-ui] shader compile error:\n%.*s\n", (int)len, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

using shader_detail::SwapShaderVersion;

} // namespace

// ---------------------------------------------------------------------------
// Batcher
// ---------------------------------------------------------------------------
Batcher::Batcher() = default;

Batcher::~Batcher() {
    Shutdown();
}

bool Batcher::Initialize() {
    if (initialized_) return true;
    if (initFailed_) return false;

    // The host (a game swap hook) does not load glad for us.
    if (!glad_glCreateShader) {
        if (gladLoaderLoadGL() == 0) {
            std::fprintf(stderr, "[aris-ui] OpenGL entry points failed to load\n");
            initFailed_ = true;
            return false;
        }
    }

    int major = 1, minor = 0;
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (ver) std::sscanf(ver, "%d.%d", &major, &minor);
    std::fprintf(stderr, "[aris-ui] GL_VERSION %s\n", ver ? ver : "(null)");

    // layout(location) needs GLSL 150. Don't hand a 2.1 driver a 330 shader;
    // some of them never return from glCompileShader.
    const char* versions[3];
    int versionCount = 0;
    if (major > 3 || (major == 3 && minor >= 3)) {
        versions[versionCount++] = "#version 330 core";
        versions[versionCount++] = "#version 330";
    }
    if (major > 3 || (major == 3 && minor >= 2))
        versions[versionCount++] = "#version 150";
    if (versionCount == 0) {
        std::fprintf(stderr, "[aris-ui] OpenGL %d.%d is below 3.2; overlay shaders skipped\n", major, minor);
        initFailed_ = true;
        return false;
    }

    GLuint vs = 0, fs = 0;
    for (int i = 0; i < versionCount; ++i) {
        const char* version = versions[i];
        const std::string vsrc = SwapShaderVersion(shaders::kVertexSrc, version);
        const std::string fsrc = SwapShaderVersion(shaders::kFragmentSrc, version);
        vs = CompileShader(GL_VERTEX_SHADER, vsrc.c_str());
        fs = CompileShader(GL_FRAGMENT_SHADER, fsrc.c_str());
        if (vs && fs) break;
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        vs = fs = 0;
    }
    if (!vs || !fs) {
        initFailed_ = true;
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei len = 0;
        glGetProgramInfoLog(program_, sizeof(log), &len, log);
        std::fprintf(stderr, "[aris-ui] program link error:\n%.*s\n", (int)len, log);
        glDeleteProgram(program_);
        program_ = 0;
        initFailed_ = true;
        return false;
    }

    uResolution_ = glGetUniformLocation(program_, "uResolution");
    uPxRange_ = glGetUniformLocation(program_, "uPxRange");
    uTex_ = glGetUniformLocation(program_, "uTex");

    // 1x1 white fallback texture (shape-only batches).
    glGenTextures(1, &whiteTexture_);
    glBindTexture(GL_TEXTURE_2D, whiteTexture_);
    const unsigned char white[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    // Vertex attribute layout (must be configured once with the VAO bound).
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = (GLsizei)sizeof(UiVertex);
    auto attr = [](GLuint index, GLint size, GLenum type, GLboolean normalized, std::size_t offset) {
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(index, size, type, normalized, stride, (const void*)offset);
    };
    attr(0, 2, GL_FLOAT, GL_FALSE, offsetof(UiVertex, pos));
    attr(1, 4, GL_FLOAT, GL_FALSE, offsetof(UiVertex, rect));
    attr(2, 4, GL_FLOAT, GL_FALSE, offsetof(UiVertex, radii));
    attr(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(UiVertex, color));
    attr(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(UiVertex, borderColor));
    attr(5, 4, GL_FLOAT, GL_FALSE, offsetof(UiVertex, params));
    attr(6, 4, GL_FLOAT, GL_FALSE, offsetof(UiVertex, shadow));
    attr(7, 4, GL_FLOAT, GL_FALSE, offsetof(UiVertex, uv));
    attr(8, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(UiVertex, color2));
    attr(9, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(UiVertex, shadowColor));
    attr(10, 4, GL_FLOAT, GL_FALSE, offsetof(UiVertex, extra));
    glBindVertexArray(0);

    initialized_ = true;
    return true;
}

void Batcher::Shutdown() {
    if (!initialized_) return;
    if (blurReady_) {
        glDeleteProgram(blurBlurProgram_);
        glDeleteProgram(blurCompProgram_);
        glDeleteVertexArrays(1, &blurVao_);
        glDeleteFramebuffers(2, blurFbo_);
        glDeleteTextures(3, blurTex_);
        blurReady_ = false;
        capW_ = capH_ = 0;
    }
    glDeleteProgram(program_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
    glDeleteTextures(1, &whiteTexture_);
    if (atlasTexture_) glDeleteTextures(1, &atlasTexture_);
    program_ = vao_ = vbo_ = whiteTexture_ = atlasTexture_ = 0;
    atlasPtr_ = nullptr;
    atlasRevision_ = 0;
    initialized_ = false;
    initFailed_ = false;
}

std::uint32_t Batcher::UploadAtlas(const FontAtlas* atlas) {
    if (!atlas || !atlas->Loaded() || !atlas->Pixels()) return 0;
    if (atlasTexture_ && atlasPtr_ == atlas && atlasRevision_ == atlas->Revision())
        return atlasTexture_;
    if (atlasTexture_) glDeleteTextures(1, &atlasTexture_);
    glGenTextures(1, &atlasTexture_);
    glBindTexture(GL_TEXTURE_2D, atlasTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, atlas->Width(), atlas->Height(), 0,
                 GL_RGB, GL_UNSIGNED_BYTE, atlas->Pixels());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    atlasPtr_ = atlas;
    atlasRevision_ = atlas->Revision();
    return atlasTexture_;
}

void Batcher::Draw(const CommandQueue& queue, const FontAtlas* atlas,
                   float framebufferWidth, float framebufferHeight) {
   if (!initialized_ && !Initialize()) return;

    // The game leaves depth, culling and a scissor active. Any one of those
    // drops the overlay. GlStateGuard restores the host state afterwards.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (framebufferWidth > 1.f && framebufferHeight > 1.f) {
        glViewport(0, 0, (GLsizei)framebufferWidth, (GLsizei)framebufferHeight);
    }

    // Premultiplied-alpha compositing. The batcher owns its blend state for the
    // UI pass; GlStateGuard restores the host's state afterwards.
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program_);
    glUniform2f(uResolution_, framebufferWidth, framebufferHeight);

    const float atlasPxRange = (atlas && atlas->Loaded()) ? atlas->PxRange() : 0.f;
    const float atlasGenSize = (atlas && atlas->Loaded()) ? atlas->GenerationFontSize() : 1.f;
    const std::uint32_t atlasTex = UploadAtlas(atlas);

    if (!scratch_) scratch_ = std::make_unique<SpanScratch>();
    BuildUiSpans(queue, *scratch_);
    const std::vector<UiSpan>& spans = scratch_->spans;
    bool scissorOn = false;
    for (const UiSpan& span : spans) {
        if (span.backdrop) {
            Rect clipStack[1];
            int depth = 0;
            if (span.scissor && span.scissorW > 0.f && span.scissorH > 0.f) {
                clipStack[0] = Rect(span.scissorX, span.scissorY, span.scissorW, span.scissorH);
                depth = 1;
            }
            DrawBackdrop(span.backdropCmd, clipStack, depth, framebufferWidth, framebufferHeight);
            scissorOn = depth > 0;
            continue;
        }
        if (span.scissor) {
            if (span.scissorW <= 0.f || span.scissorH <= 0.f) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(0, 0, 0, 0);
            } else {
                glEnable(GL_SCISSOR_TEST);
                glScissor((GLint)span.scissorX,
                          (GLint)(framebufferHeight - (span.scissorY + span.scissorH)),
                          (GLsizei)span.scissorW, (GLsizei)span.scissorH);
            }
            scissorOn = true;
        } else if (scissorOn) {
            glDisable(GL_SCISSOR_TEST);
            scissorOn = false;
        }
        Flush(span.verts, span.atlasId, span.fontSize, atlasPxRange, atlasGenSize, atlasTex);
    }
    if (scissorOn) glDisable(GL_SCISSOR_TEST);
}

void Batcher::ApplyScissor(const Rect* clipStack, int depth, float framebufferHeight) {
    if (depth <= 0) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    // Intersect the whole stack so inner clips can't exceed outer ones.
    float x0 = clipStack[0].x, y0 = clipStack[0].y;
    float x1 = x0 + clipStack[0].w, y1 = y0 + clipStack[0].h;
    for (int i = 1; i < depth; ++i) {
        x0 = std::max(x0, clipStack[i].x);
        y0 = std::max(y0, clipStack[i].y);
        x1 = std::min(x1, clipStack[i].x + clipStack[i].w);
        y1 = std::min(y1, clipStack[i].y + clipStack[i].h);
    }
    const float w = x1 - x0, h = y1 - y0;
    if (w <= 0.f || h <= 0.f) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)x0, (GLint)(framebufferHeight - y1), (GLsizei)w, (GLsizei)h);
}

void Batcher::Flush(const std::vector<UiVertex>& verts, std::uint32_t atlasId, float fontSize,
                    float atlasPxRange, float atlasGenSize, std::uint32_t atlasTex) {
    if (verts.empty()) return;

    if (std::getenv("ARIS_DEBUG")) {
        static int flushCount = 0;
        if (flushCount++ < 6) {
            const UiVertex& v = verts[0];
            std::fprintf(stderr, "[aris] flush #%d: verts=%zu atlas=%u tex=%u fontSize=%.1f pxRange=%.2f\n"
                         "  v0: rect=(%.1f,%.1f,%.1f,%.1f) uv=(%.3f,%.3f,%.3f,%.3f) isGlyph=%.0f color=%u,%u,%u,%u\n",
                         flushCount, verts.size(), atlasId, atlasTex, fontSize,
                         atlasGenSize > 0 ? atlasPxRange * (fontSize / atlasGenSize) : atlasPxRange,
                         v.rect[0], v.rect[1], v.rect[2], v.rect[3],
                         v.uv[0], v.uv[1], v.uv[2], v.uv[3], v.params[3],
                         v.color[0], v.color[1], v.color[2], v.color[3]);
        }
    }

    glUseProgram(program_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(UiVertex)),
                 verts.data(), GL_STREAM_DRAW);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(uTex_, 0);
    if (atlasId != 0 && atlasTex != 0) {
        glBindTexture(GL_TEXTURE_2D, atlasTex);
        const float px = atlasGenSize > 0.f ? atlasPxRange * (fontSize / atlasGenSize) : atlasPxRange;
        glUniform1f(uPxRange_, px);
    } else {
        glBindTexture(GL_TEXTURE_2D, whiteTexture_);
        glUniform1f(uPxRange_, 0.f);
    }

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size()));
}

// ---------------------------------------------------------------------------
// Backdrop blur ("frosted glass")
//
// Copies the already-rendered pixels under a rect, blurs them with two
// separable gaussian iterations at half resolution, then composites the result
// back through the rounded-rect mask. Everything drawn *after* the command
// (the translucent panel, text, ...) sits on the blurred pixels. All state
// touched here (FBO, viewport, blend, scissor, program) is restored; the outer
// GlStateGuard handles the rest.
// ---------------------------------------------------------------------------
namespace blur_shaders {

static const char* kQuadVs = R"GLSL(
#version 330 core
out vec2 vUv;
void main() {
    vec2 p = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

static const char* kBlurFs = R"GLSL(
#version 330 core
in vec2 vUv;
out vec4 o;
uniform sampler2D uSrc;
uniform vec2 uStep;    // tap spacing in uv units of the used region (already * direction)
uniform vec2 uUsed;    // fraction of the texture that holds valid pixels
uniform vec2 uInset;   // half-texel inset to avoid bleeding stale texels

vec4 fetch(vec2 uv) { return texture(uSrc, clamp(uv * uUsed, uInset, uUsed - uInset)); }

void main() {
    vec4 c = fetch(vUv) * 0.2270270270;
    c += fetch(vUv + uStep * 1.3846153846) * 0.3162162162;
    c += fetch(vUv - uStep * 1.3846153846) * 0.3162162162;
    c += fetch(vUv + uStep * 3.2307692308) * 0.0702702703;
    c += fetch(vUv - uStep * 3.2307692308) * 0.0702702703;
    o = c;
}
)GLSL";

static const char* kCompVs = R"GLSL(
#version 330 core
uniform vec4 uRect;          // x, y, w, h in screen px
uniform vec2 uResolution;
out vec2 vScreen;
void main() {
    vec2 p = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
    vScreen = uRect.xy - vec2(1.0) + p * (uRect.zw + vec2(2.0));
    vec2 clip = (vScreen / uResolution) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
}
)GLSL";

static const char* kCompFs = R"GLSL(
#version 330 core
in vec2 vScreen;
out vec4 o;
uniform vec4 uRect;
uniform vec4 uRadii;
uniform vec4 uRegion;     // captured region in screen px (x, y, w, h)
uniform vec2 uUsed;       // used fraction of the blurred texture
uniform vec4 uTint;       // rgb + mix amount
uniform sampler2D uTex;

float sdRoundRect(vec2 p, vec2 halfSize, vec4 radii) {
    float r = (p.x < 0.0) ? ((p.y < 0.0) ? radii.x : radii.w)
                          : ((p.y < 0.0) ? radii.y : radii.z);
    vec2 q = abs(p) - halfSize + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec2 h = uRect.zw * 0.5;
    vec2 p = vScreen - (uRect.xy + h);
    vec4 r = clamp(uRadii, vec4(0.0), vec4(min(h.x, h.y)));
    float mask = clamp(0.5 - sdRoundRect(p, h, r), 0.0, 1.0);
    if (mask <= 0.0) discard;
    vec2 uv = vec2((vScreen.x - uRegion.x) / uRegion.z, 1.0 - (vScreen.y - uRegion.y) / uRegion.w);
    vec3 c = texture(uTex, clamp(uv, 0.0, 1.0) * uUsed).rgb;
    c = mix(c, uTint.rgb, uTint.a);
    o = vec4(c * mask, mask);   // premultiplied: crossfades the backdrop under the mask
}
)GLSL";

} // namespace blur_shaders

namespace {
GLuint LinkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glDeleteProgram(prog); return 0; }
    return prog;
}
} // namespace

bool Batcher::EnsureBackdropPipeline() {
    if (blurReady_) return true;
    if (blurFailed_) return false;
    blurBlurProgram_ = LinkProgram(blur_shaders::kQuadVs, blur_shaders::kBlurFs);
    blurCompProgram_ = LinkProgram(blur_shaders::kCompVs, blur_shaders::kCompFs);
    if (!blurBlurProgram_ || !blurCompProgram_) { blurFailed_ = true; return false; }
    glGenVertexArrays(1, &blurVao_);
    glGenFramebuffers(2, blurFbo_);
    glGenTextures(3, blurTex_);
    blurReady_ = true;
    return true;
}

void Batcher::EnsureBlurTextures(int w, int h) {
    if (w <= capW_ && h <= capH_) return;
    capW_ = std::max(capW_, (w + 63) & ~63);
    capH_ = std::max(capH_, (h + 63) & ~63);
    const int hw = capW_ / 2 + 1, hh = capH_ / 2 + 1;
    const int sizes[3][2] = {{capW_, capH_}, {hw, hh}, {hw, hh}};
    for (int i = 0; i < 3; ++i) {
        glBindTexture(GL_TEXTURE_2D, blurTex_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sizes[i][0], sizes[i][1], 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, blurFbo_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               blurTex_[1 + i], 0);
    }
}

void Batcher::DrawBackdrop(const DrawCommand& cmd, const Rect* clipStack, int clipDepth,
                           float fbW, float fbH) {
    if (!EnsureBackdropPipeline()) return;
    const float radius = std::max(cmd.borderWidth, 0.f);

    // Capture region = rect grown by the blur reach, clamped to the framebuffer.
    const float pad = radius * 1.5f + 2.f;
    const int rx0 = std::max(0, (int)std::floor(cmd.pos.x - pad));
    const int ry0 = std::max(0, (int)std::floor(cmd.pos.y - pad));
    const int rx1 = std::min((int)fbW, (int)std::ceil(cmd.pos.x + cmd.size.x + pad));
    const int ry1 = std::min((int)fbH, (int)std::ceil(cmd.pos.y + cmd.size.y + pad));
    const int rw = rx1 - rx0, rh = ry1 - ry0;
    if (rw < 4 || rh < 4) return;
    const int hw = (rw + 1) / 2, hh = (rh + 1) / 2;

    GLint prevDraw = 0, prevVp[4];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
    glGetIntegerv(GL_VIEWPORT, prevVp);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);

    // 1) capture what is currently on screen under the region
    EnsureBlurTextures(rw, rh);
    glBindTexture(GL_TEXTURE_2D, blurTex_[0]);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rx0, (int)fbH - ry1, rw, rh);

    // 2) blur: (H, V) x 2 at half resolution
    const int hcw = capW_ / 2 + 1, hch = capH_ / 2 + 1;
    const float stride = std::max(radius / 11.f, 0.35f);
    glUseProgram(blurBlurProgram_);
    glBindVertexArray(blurVao_);
    glUniform1i(glGetUniformLocation(blurBlurProgram_, "uSrc"), 0);
    const GLint uStep = glGetUniformLocation(blurBlurProgram_, "uStep");
    const GLint uUsed = glGetUniformLocation(blurBlurProgram_, "uUsed");
    const GLint uInset = glGetUniformLocation(blurBlurProgram_, "uInset");
    glViewport(0, 0, hw, hh);

    struct Pass { int srcTex, dstFbo; bool horizontal; int srcW, srcH, texW, texH; };
    const Pass passes[4] = {
        {0, 0, true,  rw, rh, capW_, capH_},
        {1, 1, false, hw, hh, hcw, hch},
        {2, 0, true,  hw, hh, hcw, hch},
        {1, 1, false, hw, hh, hcw, hch},
    };
    for (const Pass& p : passes) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, blurFbo_[p.dstFbo]);
        glBindTexture(GL_TEXTURE_2D, blurTex_[p.srcTex]);
        glUniform2f(uStep, p.horizontal ? stride / (float)p.srcW : 0.f,
                    p.horizontal ? 0.f : stride / (float)p.srcH);
        glUniform2f(uUsed, (float)p.srcW / (float)p.texW, (float)p.srcH / (float)p.texH);
        glUniform2f(uInset, 0.5f / (float)p.texW, 0.5f / (float)p.texH);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // 3) composite through the rounded mask into the host framebuffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevDraw);
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);
    ApplyScissor(clipStack, clipDepth, fbH);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(blurCompProgram_);
    glBindTexture(GL_TEXTURE_2D, blurTex_[2]);
    glUniform1i(glGetUniformLocation(blurCompProgram_, "uTex"), 0);
    glUniform2f(glGetUniformLocation(blurCompProgram_, "uResolution"), fbW, fbH);
    glUniform4f(glGetUniformLocation(blurCompProgram_, "uRect"), cmd.pos.x, cmd.pos.y, cmd.size.x,
                cmd.size.y);
    glUniform4f(glGetUniformLocation(blurCompProgram_, "uRadii"), cmd.radii.x, cmd.radii.y,
                cmd.radii.z, cmd.radii.w);
    glUniform4f(glGetUniformLocation(blurCompProgram_, "uRegion"), (float)rx0, (float)ry0, (float)rw,
                (float)rh);
    glUniform2f(glGetUniformLocation(blurCompProgram_, "uUsed"), (float)hw / (float)hcw,
                (float)hh / (float)hch);
    glUniform4f(glGetUniformLocation(blurCompProgram_, "uTint"), cmd.color.x, cmd.color.y, cmd.color.z,
                cmd.color.w);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // restore what the main batch expects
    glUseProgram(program_);
    glBindVertexArray(vao_);
}

} // namespace ui