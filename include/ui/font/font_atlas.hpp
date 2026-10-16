#pragma once
// aris-ui — runtime MSDF font atlas.
// Atlases are generated OFFLINE (tools/atlas_generator) and loaded at runtime
// as PNG + JSON metadata only. Metrics are em-normalized so glyphs render
// crisply at any target font size / DPI scale.
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "ui/core/math.hpp"

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
    // Pixels stay on the CPU. The active renderer uploads them.
    bool Load(const std::string& pngPath, const std::string& jsonPath);

    // Direct TrueType/OpenType font loading (.ttf, .otf, or Google Fonts).
    // Automatically checks cache under cachePrefix (defaults to "atlas/<font-name>"),
    // generates the MSDF atlas if missing or outdated, and keeps the pixels on the CPU.
    bool LoadTtf(const std::string& ttfPath,
                 const std::string& cachePrefix = "",
                 float fontSize = 48.f,
                 float pxRange = 4.f);

    void Unload();
    bool Loaded() const { return !glyphs_.empty() && !pixels_.empty(); }

    // Stable non-zero id while loaded. Renderers bind their own GPU texture;
    // this value only groups glyph draws in the command stream.
    std::uint32_t TextureId() const { return Loaded() ? 1u : 0u; }
    const std::uint8_t* Pixels() const { return pixels_.empty() ? nullptr : pixels_.data(); }
    int Width() const { return width_; }
    int Height() const { return height_; }
    int Channels() const { return 3; }
    std::uint64_t Revision() const { return revision_; }
    float PxRange() const { return pxRange_; }
    float GenerationFontSize() const { return fontSize_; }
    float LineHeight(float fontSize) const { return lineHeight_ * fontSize; }
    float Ascender(float fontSize) const { return ascender_ * fontSize; }

    const GlyphInfo* GetGlyph(std::uint32_t codepoint) const;

private:
    std::unordered_map<std::uint32_t, GlyphInfo> glyphs_;
    std::vector<std::uint8_t> pixels_;
    std::uint64_t revision_ = 0;
    int width_ = 0, height_ = 0;
    float pxRange_ = 0.f;
    float fontSize_ = 0.f;
    float lineHeight_ = 0.f;
    float ascender_ = 0.f;
    float descender_ = 0.f;
};

} // namespace ui