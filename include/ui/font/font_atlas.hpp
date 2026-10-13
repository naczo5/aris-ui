#pragma once
// aris-ui — runtime MSDF font atlas.
// Atlases are generated OFFLINE (tools/atlas_generator) and loaded at runtime
// as PNG + JSON metadata only. Metrics are em-normalized so glyphs render
// crisply at any target font size / DPI scale.
#include <cstdint>
#include <string>
#include <unordered_map>
#include "ui/core/math.hpp"
#include "glad/gl.h"

namespace ui {

struct GlyphInfo {
    float advance = 0.f;    // em-normalized: fraction of font size
    float left = 0.f;       // plane offset from pen, em-normalized (x right)
    float top = 0.f;        // plane offset from baseline, em-normalized (y up)
    float width = 0.f;      // quad width, em-normalized
    float height = 0.f;     // quad height, em-normalized
    float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f;  // atlas uv (top-left origin)
};

class FontAtlas {
public:
    FontAtlas() = default;
    ~FontAtlas();

    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    // pngPath: generated MSDF atlas image; jsonPath: generated metadata.
    bool Load(const std::string& pngPath, const std::string& jsonPath);
    void Unload();
    bool Loaded() const { return texture_ != 0; }

    std::uint32_t TextureId() const { return texture_; }
    float PxRange() const { return pxRange_; }
    float GenerationFontSize() const { return fontSize_; }
    float LineHeight(float fontSize) const { return lineHeight_ * fontSize; }
    float Ascender(float fontSize) const { return ascender_ * fontSize; }

    const GlyphInfo* GetGlyph(std::uint32_t codepoint) const;

private:
    std::unordered_map<std::uint32_t, GlyphInfo> glyphs_;
    std::uint32_t texture_ = 0;
    int width_ = 0, height_ = 0;
    float pxRange_ = 0.f;
    float fontSize_ = 0.f;
    float lineHeight_ = 0.f;
    float ascender_ = 0.f;
    float descender_ = 0.f;
};

} // namespace ui