# Vendored dependencies (pinned)

| Library | Version | Source | Purpose |
|---|---|---|---|
| yoga | v3.2.1 | github.com/facebook/yoga | headless flexbox layout |
| msdfgen | v1.12.1 | github.com/Chlumsky/msdfgen | multi-channel SDF generation (atlas tool) |
| glad | glad2, generated GL core 3.3 | github.com/Dav1dde/glad | OpenGL loader |
| stb | master snapshot 2026-10 | github.com/nothings/stb | stb_image (atlas loading), stb_image_write (atlas output) |
| nlohmann/json | v3.11.3 single header | github.com/nlohmann/json | atlas metadata parsing |
| Vulkan-Headers | 1.4.356 | github.com/KhronosGroup/Vulkan-Headers | host-owned Vulkan renderer (headers only, no loader lib) |

Notes:
- Vendored repos were trimmed of language bindings/tests/fuzz assets; `.git` folders removed.
- Regenerate `glad/` with: `pip install glad2 && glad --api="gl:core=3.3" --out-path=glad --extensions="" c --loader`
- GLFW is *not* vendored; it is fetched via CMake FetchContent for the desktop runner example only.