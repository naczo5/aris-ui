// aris-ui atlas generator CLI
//
//   atlasgen --font <path.ttf> [--out <prefix>] [--size 48] [--range 4]
//            [--from 32] [--to 126] [--extra "<utf8 chars>"]
//
// Writes <prefix>.png (RGB MSDF atlas) + <prefix>.json (em-normalized metadata).
#include "atlas_generator/atlas_gen.hpp"

#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    std::string fontPath, outPrefix = "atlas";
    ui_atlas::Params params;

    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : ""; };
        if (!std::strcmp(argv[i], "--font")) fontPath = next();
        else if (!std::strcmp(argv[i], "--out")) outPrefix = next();
        else if (!std::strcmp(argv[i], "--size")) params.fontSize = (float)std::atof(next());
        else if (!std::strcmp(argv[i], "--range")) params.pxRange = (float)std::atof(next());
        else if (!std::strcmp(argv[i], "--from")) params.firstCodepoint = std::atoi(next());
        else if (!std::strcmp(argv[i], "--to")) params.lastCodepoint = std::atoi(next());
        else if (!std::strcmp(argv[i], "--extra")) params.extraChars = next();
        else if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
            std::printf("usage: atlasgen --font <path.ttf> [--out <prefix>] [--size 48]\n"
                        "                [--range 4] [--from 32] [--to 126] [--extra \"<utf8>\"]\n");
            return 0;
        }
    }

    if (fontPath.empty()) {
        std::fprintf(stderr, "error: --font is required\n");
        return 1;
    }

    std::string error;
    if (!ui_atlas::WriteFiles(fontPath, outPrefix, params, error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    std::printf("atlas written: %s.png + %s.json\n", outPrefix.c_str(), outPrefix.c_str());
    return 0;
}