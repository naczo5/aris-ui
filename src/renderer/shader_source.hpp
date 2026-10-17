#pragma once
// aris-ui — internal shader-source helpers (OpenGL backend).
// Kept separate from batcher.cpp so the version-swap logic is unit-testable
// without a GL context.
#include <cstring>
#include <string>

namespace ui {
namespace shader_detail {

// Replaces the first line of a shader source with `versionLine`.
//
// The built-in sources are raw string literals that start with a leading
// newline for readability. That newline must be skipped before locating the
// end of the first line, otherwise the original "#version" directive is left
// in place and the directive ends up duplicated — which every GLSL compiler
// rejects ("version directive must be first statement and may not be
// repeated") and which silently disables the whole OpenGL backend.
inline std::string SwapShaderVersion(const char* src, const char* versionLine) {
    const char* begin = src;
    while (*begin == '\n' || *begin == '\r') ++begin;
    const char* nl = std::strchr(begin, '\n');
    if (!nl) return src;
    std::string out = versionLine;
    out.append(nl);
    return out;
}

} // namespace shader_detail
} // namespace ui
