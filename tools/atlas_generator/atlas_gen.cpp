#include "atlas_generator/atlas_gen.hpp"

#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <nlohmann/json.hpp>

#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ui_atlas {

namespace {

struct Placement {
    int x = 0, y = 0, w = 0, h = 0;   // PNG pixel coords, top-left origin
};

// JSON-escape a string for embedding in the hand-written metadata document.
// Windows font paths contain backslashes; writing them raw produced invalid
// JSON ("\U" is not an escape) and every FontAtlas::Load failed at runtime.
std::string EscapeJson(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const unsigned char c : text) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(c));
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
            break;
        }
    }
    return out;
}

// Simple shelf packer.
class ShelfPacker {
public:
    explicit ShelfPacker(int startWidth) : width_(startWidth) {}

    bool TryPlace(int w, int h, int pad, Placement& out) {
        for (int attempt = 0; attempt < 4; ++attempt) {
            if (cursorX_ + w + pad <= width_ && cursorY_ + h + pad <= height_) {
                out = Placement{cursorX_, cursorY_, w, h};
                cursorX_ += w + pad;
                rowH_ = std::max(rowH_, h + pad);
                return true;
            }
            if (cursorX_ + w + pad > width_ && cursorY_ + rowH_ + h + pad <= height_) {
                cursorY_ += rowH_;
                cursorX_ = 0;
                rowH_ = 0;
                continue;
            }
            Grow();
        }
        return false;
    }

    int Width() const { return width_; }
    int Height() const { return height_; }

private:
    void Grow() {
        width_ *= 2;
        height_ *= 2;
    }

    int width_, height_ = 256;
    int cursorX_ = 0, cursorY_ = 0, rowH_ = 0;
};

} // namespace

bool Generate(const std::string& fontPath, const Params& params, Result& out, std::string& error) {
    msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
    if (!ft) {
        error = "failed to initialize FreeType";
        return false;
    }
    msdfgen::FontHandle* font = msdfgen::loadFont(ft, fontPath.c_str());
    if (!font) {
        error = "failed to load font: " + fontPath;
        deinitializeFreetype(ft);
        return false;
    }

    msdfgen::FontMetrics metrics;
    if (!msdfgen::getFontMetrics(metrics, font, msdfgen::FONT_SCALING_EM_NORMALIZED)) {
        error = "failed to read font metrics";
        destroyFont(font);
        deinitializeFreetype(ft);
        return false;
    }

    out = Result{};
    out.lineHeight = metrics.lineHeight;      // em-normalized (emSize == 1)
    out.ascender = metrics.ascenderY;
    out.descender = metrics.descenderY;

    const float fs = params.fontSize;
    const int margin = static_cast<int>(std::ceil(params.pxRange));
    const int packPad = margin + 1;           // extra separation against bleeding

    ShelfPacker packer(256);

    struct StagedGlyph {
        Placement place;
        int bw, bh;
        msdfgen::Bitmap<float, 3> msdf;
    };
    std::vector<StagedGlyph> staged;

    auto emitGlyph = [&](std::uint32_t cp) -> bool {
        msdfgen::Shape shape;
        double advance = 0.0;
        if (!msdfgen::loadGlyph(shape, font, cp, msdfgen::FONT_SCALING_EM_NORMALIZED, &advance)) {
            return false;   // glyph missing in font — skip silently
        }
        shape.normalize();
        shape.orientContours();

        GlyphResult result;
        result.codepoint = cp;
        result.advance = advance;

        if (shape.contours.empty()) {
            // Whitespace glyph: advance only, empty quad.
            result.plane[0] = result.plane[1] = result.plane[2] = result.plane[3] = 0.0;
            result.atlas[0] = result.atlas[1] = result.atlas[2] = result.atlas[3] = 0.0;
            out.glyphs.push_back(result);
            return true;
        }

        msdfgen::edgeColoringSimple(shape, 3.0);

        const msdfgen::Shape::Bounds bounds = shape.getBounds();
        const double gw = std::ceil((bounds.r - bounds.l) * fs) + 2 * margin;
        const double gh = std::ceil((bounds.t - bounds.b) * fs) + 2 * margin;
        const int bw = static_cast<int>(gw);
        const int bh = static_cast<int>(gh);
        if (bw <= 0 || bh <= 0) {
            result.plane[0] = result.plane[1] = result.plane[2] = result.plane[3] = 0.0;
            result.atlas[0] = result.atlas[1] = result.atlas[2] = result.atlas[3] = 0.0;
            out.glyphs.push_back(result);
            return true;
        }

        Placement place;
        if (!packer.TryPlace(bw, bh, packPad, place)) {
            error = "atlas packing failed";
            return false;
        }

        // Projection: shape (em) -> bitmap pixels. msdfgen v1.12 semantic:
        //   pixel = scale * (shape + translate)   — translate in SHAPE units.
        // Glyph's bottom-left lands at (margin, margin) inside its bitmap cell.
        const msdfgen::Projection projection(
            msdfgen::Vector2(fs, fs),
            msdfgen::Vector2(margin / fs - bounds.l, margin / fs - bounds.b));

        // msdfgen stores value = (d - lower) / (upper - lower) where d is the
        // signed distance measured in SHAPE units — the range must be given in
        // the same units (em), not pixels: pxRange / fs em == pxRange px.
        const msdfgen::Range range(params.pxRange / fs);

        msdfgen::Bitmap<float, 3> msdf(bw, bh);
        msdfgen::generateMSDF(msdf, shape, projection, range);
        msdfgen::distanceSignCorrection(msdf, shape, projection, msdfgen::FILL_NONZERO);
        msdfgen::msdfErrorCorrection(msdf, shape, projection, range);

        // Stage the bitmap; blitting happens after ALL placements so the atlas
        // stride matches the packer's final size (the packer may grow mid-run,
        // and blitting early would corrupt rows for glyphs packed before that).
        staged.push_back(StagedGlyph{place, bw, bh, std::move(msdf)});

        // Standard msdf-atlas-gen bounds:
        // planeBounds represents the em-normalized quad covering the ENTIRE
        // padded cell (including the distance field margin).
        // atlasBounds represents the exact pixel rectangle of that cell in the PNG.
        result.plane[0] = static_cast<double>(bounds.l) - static_cast<double>(margin) / fs;
        result.plane[1] = static_cast<double>(bounds.b) - static_cast<double>(margin) / fs;
        result.plane[2] = result.plane[0] + static_cast<double>(bw) / fs;
        result.plane[3] = result.plane[1] + static_cast<double>(bh) / fs;

        result.atlas[0] = static_cast<double>(place.x);
        result.atlas[1] = static_cast<double>(place.y);
        result.atlas[2] = static_cast<double>(bw);
        result.atlas[3] = static_cast<double>(bh);
        out.glyphs.push_back(result);
        return true;
    };

    bool ok = true;
    for (int cp = params.firstCodepoint; cp <= params.lastCodepoint && ok; ++cp) {
        ok = emitGlyph(static_cast<std::uint32_t>(cp));
    }
    // Decode UTF-8 extras into codepoints.
    const std::string& extras = params.extraChars;
    for (size_t i = 0; i < extras.size() && ok;) {
        const unsigned char c = static_cast<unsigned char>(extras[i]);
        std::uint32_t cp = 0;
        int len = 1;
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { cp = c; }
        bool complete = true;
        for (int k = 1; k < len; ++k) {
            if (i + k < extras.size()) cp = (cp << 6) | (static_cast<unsigned char>(extras[i + k]) & 0x3F);
            else complete = false;
        }
        if (complete) ok = emitGlyph(cp);
        i += static_cast<size_t>(len);
    }

    // Blit all staged glyphs with the FINAL atlas dimensions (bitmap origin is
    // bottom-left; PNG is top-down).
    const int atlasW = packer.Width(), atlasH = packer.Height();
    out.width = atlasW;
    out.height = atlasH;
    out.pixels.assign((size_t)atlasW * atlasH * 3, 0);
    for (const StagedGlyph& g : staged) {
        for (int row = 0; row < g.bh; ++row) {
            const int pngRow = g.place.y + g.bh - 1 - row;   // flip: bitmap row 0 is bottom
            for (int col = 0; col < g.bw; ++col) {
                const float* src = g.msdf(col, row);         // msdf(x = column, y = row)
                auto* dst = &out.pixels[(size_t)(pngRow * atlasW + g.place.x + col) * 3];
                for (int ch = 0; ch < 3; ++ch) {
                    dst[ch] = static_cast<unsigned char>(std::clamp(src[ch], 0.0f, 1.0f) * 255.f + 0.5f);
                }
            }
        }
    }

    destroyFont(font);
    deinitializeFreetype(ft);

    if (!ok && error.empty()) error = "glyph generation failed";
    return ok;
}

bool WriteFiles(const std::string& fontPath, const std::string& outPrefix,
                const Params& params, std::string& error) {
    Result result;
    if (!Generate(fontPath, params, result, error)) return false;

    const std::string pngPath = outPrefix + ".png";
    const std::string jsonPath = outPrefix + ".json";

    stbi_write_png(pngPath.c_str(), result.width, result.height, 3, result.pixels.data(),
                   result.width * 3);

    std::ostringstream json;
    json << "{\n";
    json << "  \"atlas\": {\"width\": " << result.width << ", \"height\": " << result.height << "},\n";
    json << "  \"pxRange\": " << params.pxRange << ",\n";
    json << "  \"font\": \"" << EscapeJson(fontPath) << "\",\n";
    json << "  \"metrics\": {\"fontSize\": " << params.fontSize
         << ", \"lineHeight\": " << result.lineHeight
         << ", \"ascender\": " << result.ascender
         << ", \"descender\": " << result.descender << "},\n";
    json << "  \"glyphs\": [\n";
    for (size_t i = 0; i < result.glyphs.size(); ++i) {
        const GlyphResult& g = result.glyphs[i];
        json << "    {\"codepoint\": " << g.codepoint
             << ", \"advance\": " << g.advance
             << ", \"planeBounds\": [" << g.plane[0] << ", " << g.plane[1] << ", "
             << g.plane[2] << ", " << g.plane[3] << "]"
             << ", \"atlasBounds\": [" << g.atlas[0] << ", " << g.atlas[1] << ", "
             << g.atlas[2] << ", " << g.atlas[3] << "]}";
        json << (i + 1 < result.glyphs.size() ? ",\n" : "\n");
    }
    json << "  ]\n}\n";

    const std::string payload = json.str();
    // Safety net: the runtime loader hard-fails on malformed metadata, so never
    // write a document that does not parse as JSON.
    try {
        const nlohmann::json parsed = nlohmann::json::parse(payload);
        (void)parsed;
    } catch (const std::exception& e) {
        error = std::string("generated atlas JSON is not valid JSON: ") + e.what();
        return false;
    }

    std::ofstream out(jsonPath);
    if (!out) {
        error = "cannot write " + jsonPath;
        return false;
    }
    out << payload;
    return true;
}

bool GenerateAndWrite(const std::string& fontPath, const std::string& outPrefix,
                      const Params& params, std::string& error) {
    namespace fs = std::filesystem;
    // Cache check: skip regeneration when outputs exist and are newer than the font.
    const std::string pngPath = outPrefix + ".png";
    const std::string jsonPath = outPrefix + ".json";
    std::error_code ec;
    const auto fontTime = fs::last_write_time(fontPath, ec);
    const auto pngTime = fs::last_write_time(pngPath, ec);
    if (!ec && pngTime >= fontTime) {
        std::ifstream jsonFile(jsonPath);
        if (jsonFile.good()) {
            // Cached only if it was built from this exact font file.
            std::stringstream ss;
            ss << jsonFile.rdbuf();
            if (ss.str().find("\"font\": \"" + EscapeJson(fontPath) + "\"") != std::string::npos)
                return true;   // cached
        }
    }
    return WriteFiles(fontPath, outPrefix, params, error);
}

} // namespace ui_atlas