# minecraft_hook (future stage)

Minimal C++ injected-DLL example showing `wglSwapBuffers` (Windows) and
`glXSwapBuffers` / `eglSwapBuffers` (Linux) hooking with a `WndProc` input
adapter feeding `ui::Context`.

Planned for Stage 12 — see [../../PROGRESS.md](../../PROGRESS.md).

Sketch of the planned integration:

```cpp
// swapbuffer hook (called by the host's render loop)
extern ui::Context g_ctx;

LONG_PTR WndProcAdapter(HWND, UINT msg, WPARAM, LPARAM) {
    // translate Win32 messages -> ui::InputEvent, forward to g_ctx.ProcessInput
    // return "consumed" to block game input while the UI is open
}

void OnSwapBuffers(HDC hdc) {
    g_ctx.BeginFrame(dt, uiWidth, uiHeight);
    // ... build UI ...
    g_ctx.Render();   // GlStateGuard keeps Minecraft's pipeline intact
}
```

Minecraft helper utilities (GUI-scale sync, LWJGL2/GLFW cursor control)
will live in `include/ui/helpers/` once this stage lands.