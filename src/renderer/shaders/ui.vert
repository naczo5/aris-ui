#version 450
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aRect;
layout(location = 2) in vec4 aRadii;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aBorderColor;
layout(location = 5) in vec4 aParams;
layout(location = 6) in vec4 aShadow;
layout(location = 7) in vec4 aUv;
layout(location = 8) in vec4 aColor2;
layout(location = 9) in vec4 aShadowColor;
layout(location = 10) in vec4 aExtra;

layout(push_constant) uniform PushConstants {
    vec2 uResolution;
    float uPxRange;
    float pad;
} pc;

layout(location = 0) out vec2 vPos01;
layout(location = 1) out vec2 vLocal;
layout(location = 2) out vec2 vHalf;
layout(location = 3) out vec4 vRadii;
layout(location = 4) out vec4 vColor;
layout(location = 5) out vec4 vBorderColor;
layout(location = 6) out vec4 vParams;
layout(location = 7) out vec4 vShadow;
layout(location = 8) out vec2 vGlyphUv;
layout(location = 9) out vec4 vColor2;
layout(location = 10) out vec4 vShadowColor;
layout(location = 11) out vec4 vExtra;

void main() {
    vPos01 = aPos;
    vec2 size = aRect.zw + 2.0 * vec2(aShadow.w);
    vec2 screen = aRect.xy - vec2(aShadow.w) + aPos * size;
    vLocal = aPos * size - vec2(aShadow.w);
    vHalf = aRect.zw * 0.5;
    vRadii = aRadii;
    vColor = aColor;
    vBorderColor = aBorderColor;
    vParams = aParams;
    vShadow = aShadow;
    vColor2 = aColor2;
    vShadowColor = aShadowColor;
    vExtra = aExtra;
    vGlyphUv = vec2(mix(aUv.x, aUv.z, aPos.x), mix(aUv.y, aUv.w, aPos.y));
    // Vulkan framebuffer y grows downward; NDC -1 is the top with a positive viewport.
    vec2 clip = (screen / pc.uResolution) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, clip.y, 0.0, 1.0);
}
