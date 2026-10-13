#pragma once
// aris-ui atlas generator — offline MSDF glyph atlas builder (FreeType + msdfgen).
// Produces <prefix>.png (RGB MSDF atlas) + <prefix>.json (em-normalized metadata).
#include <cstdint>
#include <string>
#include <vector>

namespace ui_atlas {

struct Params {
    float fontSize = 48.f;      // reference size in px (em-normalization basis)
    float pxRange = 4.f;        // MSDF distance range in atlas pixels
    int firstCodepoint = 32;    // ASCII printable range by default
    int lastCodepoint = 126;
    std::string extraChars = "\xC3\x97\xE2\x80\x94\xE2\x9C\x93";   // UTF-8 extras (×, —, ✓)
};

struct GlyphResult {
    std::uint32_t codepoint;
    double advance;             // em-normalized
    double plane[4];            // l, b, r, t (em-normalized, y-up)
    double atlas[4];            // x, y, w, h (PNG pixels, top-left origin)
};

struct Result {
    int width = 0, height = 0;
    std::vector<GlyphResult> glyphs;
    std::vector<unsigned char> pixels;   // packed RGB MSDF atlas (top-down rows)
    double lineHeight = 0;      // em-normalized
    double ascender = 0;        // em-normalized
    double descender = 0;       // em-normalized
};

// Generates the atlas in memory.
bool Generate(const std::string& fontPath, const Params& params, Result& out, std::string& error);

// Writes <prefix>.png + <prefix>.json. Returns false on failure.
bool WriteFiles(const std::string& fontPath, const std::string& outPrefix,
                const Params& params, std::string& error);

// Convenience: generate + write (skips writing if files exist and font is unchanged).
bool GenerateAndWrite(const std::string& fontPath, const std::string& outPrefix,
                      const Params& params, std::string& error);

} // namespace ui_atlas