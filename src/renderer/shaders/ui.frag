#version 450
layout(location = 0) in vec2 vPos01;
layout(location = 1) in vec2 vLocal;
layout(location = 2) in vec2 vHalf;
layout(location = 3) in vec4 vRadii;
layout(location = 4) in vec4 vColor;
layout(location = 5) in vec4 vBorderColor;
layout(location = 6) in vec4 vParams;
layout(location = 7) in vec4 vShadow;
layout(location = 8) in vec2 vGlyphUv;
layout(location = 9) in vec4 vColor2;
layout(location = 10) in vec4 vShadowColor;
layout(location = 11) in vec4 vExtra;

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(push_constant) uniform PushConstants {
    vec2 uResolution;
    float uPxRange;
    float pad;
} pc;

layout(location = 0) out vec4 fragColor;

float sdRoundRect(vec2 p, vec2 halfSize, vec4 radii) {
    float r = (p.x < 0.0)
        ? ((p.y < 0.0) ? radii.x : radii.w)
        : ((p.y < 0.0) ? radii.y : radii.z);
    vec2 q = abs(p) - halfSize + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float median3(vec3 v) {
    return max(min(v.x, v.y), min(max(v.x, v.y), v.z));
}

float sdTriangle(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 e0 = b - a, e1 = c - b, e2 = a - c;
    vec2 v0 = p - a, v1 = p - b, v2 = p - c;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / max(dot(e0, e0), 1e-6), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / max(dot(e1, e1), 1e-6), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / max(dot(e2, e2), 1e-6), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                     vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

float screenPxRange() {
    vec2 texSize = vec2(textureSize(uTex, 0));
    vec2 range = vec2(pc.uPxRange) / texSize;
    vec2 screenTexSize = vec2(1.0) / fwidth(vGlyphUv);
    return max(0.5 * dot(range, screenTexSize), 1.0);
}

void main() {
    if (vParams.w > 0.5 && vParams.w < 1.5) {
        vec3 msdf = texture(uTex, vGlyphUv).rgb;
        float sd = median3(msdf) - 0.5;
        if (sd <= -0.42) discard;
        float screenPxDistance = screenPxRange() * sd;
        float alpha = clamp(screenPxDistance + 0.5 + vParams.x, 0.0, 1.0);
        float a = vColor.a * alpha;
        if (a < 0.004) discard;
        fragColor = vec4(vColor.rgb * a, a);
        return;
    }

    if (vParams.w > 2.5) {
        float sd = sdTriangle(vLocal, vRadii.xy, vRadii.zw, vExtra.xy);
        float d = (vParams.x > 0.0) ? abs(sd) - vParams.x * 0.5 : sd;
        float alpha = clamp(0.5 - d, 0.0, 1.0);
        float ta = vColor.a * alpha;
        if (ta < 0.004) discard;
        fragColor = vec4(vColor.rgb * ta, ta);
        return;
    }

    if (vParams.w > 1.5) {
        vec2 a = vRadii.xy, b = vRadii.zw;
        vec2 pa = vLocal - a, ba = b - a;
        float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
        float d = length(pa - ba * h) - vParams.x * 0.5;
        float alpha = clamp(0.5 - d, 0.0, 1.0);
        float la = vColor.a * alpha;
        if (la < 0.004) discard;
        fragColor = vec4(vColor.rgb * la, la);
        return;
    }

    vec2 p = vLocal - vHalf;
    float maxR = min(vHalf.x, vHalf.y);
    vec4 radii = clamp(vRadii, vec4(0.0), vec4(maxR));
    float sd = sdRoundRect(p, vHalf, radii);
    float aa = max(fwidth(sd), 1e-4);
    float fill = 1.0 - smoothstep(-aa, aa, sd);

    vec4 fillCol = vColor;
    if (dot(vExtra.xy, vExtra.xy) > 0.0) {
        vec2 uv = (vLocal / (2.0 * vHalf)) - 0.5;
        float t = dot(uv, vExtra.xy) / max(abs(vExtra.x) + abs(vExtra.y), 1e-4) + 0.5;
        fillCol = mix(vColor, vColor2, clamp(t, 0.0, 1.0));
    }

    float bw = max(vParams.x, 0.0);
    float borderA = 0.0;
    if (bw > 0.0 && vBorderColor.a > 0.0) {
        borderA = fill * smoothstep(-bw - aa, -bw + aa, sd);
    }

    float shadowA = 0.0;
    if (vShadow.z > 0.0) {
        vec2 sp = p - vShadow.xy;
        float ssd = sdRoundRect(sp, vHalf + vec2(vParams.y), radii);
        float blur = max(vParams.z, 0.001);
        float k = clamp(0.5 - ssd / (2.0 * blur), 0.0, 1.0);
        shadowA = (k * k * (3.0 - 2.0 * k)) * vShadow.z * vShadowColor.a * (1.0 - fill);
    }

    vec4 acc = vec4(vShadowColor.rgb * shadowA, shadowA);
    float fa = fillCol.a * fill;
    acc = vec4(fillCol.rgb * fa, fa) + acc * (1.0 - fa);
    float ba = vBorderColor.a * borderA;
    acc = vec4(vBorderColor.rgb * ba, ba) + acc * (1.0 - ba);
    if (acc.a < 0.004) discard;
    fragColor = acc;
}
