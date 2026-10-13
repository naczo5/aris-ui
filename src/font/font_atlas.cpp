#include "ui/font/font_atlas.hpp"

#include <nlohmann/json.hpp>
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#if defined(ARIS_UI_HAS_ATLASGEN)
#include "atlas_generator/atlas_gen.hpp"
#include <filesystem>
#endif

namespace ui {

using json = nlohmann::json;

FontAtlas::~FontAtlas() {
    Unload();
}

bool FontAtlas::Load(const std::string& pngPath, const std::string& jsonPath) {
    Unload();

    // --- metadata ---
    std::ifstream file(jsonPath);
    if (!file) {
        std::fprintf(stderr, "[aris-ui] FontAtlas: cannot open %s\n", jsonPath.c_str());
        return false;
    }
    json meta;
    try {
        file >> meta;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[aris-ui] FontAtlas: bad json (%s): %s\n", jsonPath.c_str(), e.what());
        return false;
    }

    width_ = meta.at("atlas").at("width").get<int>();
    height_ = meta.at("atlas").at("height").get<int>();
    pxRange_ = meta.at("pxRange").get<float>();
    fontSize_ = meta.at("metrics").at("fontSize").get<float>();
    lineHeight_ = meta.at("metrics").at("lineHeight").get<float>();
    ascender_ = meta.at("metrics").at("ascender").get<float>();
    descender_ = meta.at("metrics").at("descender").get<float>();

    // --- image ---
    int w = 0, h = 0, n = 0;
    unsigned char* pixels = stbi_load(pngPath.c_str(), &w, &h, &n, 3);
    if (!pixels) {
        std::fprintf(stderr, "[aris-ui] FontAtlas: cannot load %s\n", pngPath.c_str());
        return false;
    }
    if (w != width_ || h != height_) {
        std::fprintf(stderr, "[aris-ui] FontAtlas: image %dx%d does not match metadata %dx%d\n",
                     w, h, width_, height_);
        stbi_image_free(pixels);
        return false;
    }

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);

    // --- glyphs (em-normalized plane metrics, pixel atlas bounds) ---
    const float atlasW = static_cast<float>(width_);
    const float atlasH = static_cast<float>(height_);
    for (const auto& g : meta.at("glyphs")) {
        GlyphInfo info;
        info.advance = g.at("advance").get<float>();
        const auto pb = g.at("planeBounds");
        const auto ab = g.at("atlasBounds");
        const float l = pb[0].get<float>(), b = pb[1].get<float>();
        const float r = pb[2].get<float>(), t = pb[3].get<float>();
        info.left = l;
        info.top = t;
        info.width = r - l;
        info.height = t - b;
        const float ax = ab[0].get<float>(), ay = ab[1].get<float>();
        const float aw = ab[2].get<float>(), ah = ab[3].get<float>();
        info.u0 = ax / atlasW;
        info.v0 = ay / atlasH;               // glyph top edge in the PNG
        info.u1 = (ax + aw) / atlasW;
        info.v1 = (ay + ah) / atlasH;
        glyphs_[g.at("codepoint").get<std::uint32_t>()] = info;
    }
    return true;
}

bool FontAtlas::LoadTtf(const std::string& ttfPath, const std::string& cachePrefix,
                        float fontSize, float pxRange) {
#if defined(ARIS_UI_HAS_ATLASGEN)
    namespace fs = std::filesystem;
    std::string prefix = cachePrefix;
    if (prefix.empty()) {
        fs::path p(ttfPath);
        std::string stem = p.stem().string();
        prefix = "atlas/" + stem;
    }
    std::error_code ec;
    fs::create_directories(fs::path(prefix).parent_path(), ec);
    ui_atlas::Params params;
    params.fontSize = fontSize;
    params.pxRange = pxRange;
    std::string err;
    if (!ui_atlas::GenerateAndWrite(ttfPath, prefix, params, err)) {
        std::fprintf(stderr, "[aris-ui] FontAtlas::LoadTtf: failed to build atlas for %s: %s\n",
                     ttfPath.c_str(), err.c_str());
        return false;
    }
    return Load(prefix + ".png", prefix + ".json");
#else
    (void)ttfPath; (void)cachePrefix; (void)fontSize; (void)pxRange;
    std::fprintf(stderr, "[aris-ui] FontAtlas::LoadTtf: atlas generator not enabled (requires ARIS_UI_BUILD_TOOLS=ON)\n");
    return false;
#endif
}

void FontAtlas::Unload() {
    if (texture_) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    glyphs_.clear();
}

const GlyphInfo* FontAtlas::GetGlyph(std::uint32_t codepoint) const {
    auto it = glyphs_.find(codepoint);
    return it == glyphs_.end() ? nullptr : &it->second;
}

} // namespace ui