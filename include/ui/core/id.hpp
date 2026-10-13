#pragma once
// aris-ui — widget IDs and ID-stack hashing (FNV-1a 64)
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace ui {

using WidgetId = std::uint64_t;
constexpr WidgetId kInvalidId = 0;

inline WidgetId HashBytes(WidgetId seed, const void* data, std::size_t len) {
    const auto* p = static_cast<const unsigned char*>(data);
    WidgetId h = seed;
    for (std::size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

inline WidgetId HashCombine(WidgetId seed, std::uint64_t v) {
    return HashBytes(seed, &v, sizeof(v));
}

inline WidgetId HashString(WidgetId seed, const char* s) {
    return s ? HashBytes(seed, s, std::strlen(s)) : seed;
}

inline WidgetId HashPointer(WidgetId seed, const void* p) {
    return HashCombine(seed, reinterpret_cast<std::uintptr_t>(p));
}

} // namespace ui