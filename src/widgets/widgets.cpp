#include "ui/widgets/widgets.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {
// Blends `a -> b -> c` with independent weights.
Color Blend2(const Color& a, const Color& b, float t1, const Color& c, float t2) {
    Color mid = Color::Blend(a, b, t1);
    return Color::Blend(mid, c, t2);
}
} // namespace

// ---------------------------------------------------------------------------
bool Button(Context& ctx, const char* label) {
    const Theme& th = ctx.theme();
    WidgetFrame w = ctx.BeginWidget(label, th.controlHeight);

    if (w.rect.Valid()) {
        const float hov = w.state->hoverT, prs = w.state->pressT;
        RectStyle st;
        st.fill = Color::Blend(th.accent, th.accentHover, hov);
        st.fillEnd = Color::Blend(th.accentActive, th.accent, hov * 0.5f);
        st.gradientDir = Vec2(0.6f, 1.f);
        st.fill = Color::Blend(st.fill, th.accentActive, prs * 0.6f);
        st.cornerRadii = Vec4(th.controlRadius);
        st.borderColor = Color(255, 255, 255, (std::uint8_t)(38 + 40 * hov));
        st.borderWidth = 1.f;
        // Hover lifts the button and intensifies the glow; press settles it down.
        st.shadowColor = th.accent;
        st.shadowOffset = Vec2(0.f, Lerp(3.f, 6.f, hov) - 3.f * prs);
        st.shadowBlur = 14.f + 6.f * hov;
        st.shadowOpacity = 0.30f + 0.30f * hov - 0.20f * prs;
        ctx.DrawRect(w.rect, st);

        const float fs = th.fontSize;
        const Vec2 ts = ctx.MeasureText(label, fs);
        const Vec2 c = w.rect.Center();
        const float capH = fs * 0.72f;
        const float textTop = c.y - capH * 0.5f - (fs - capH) * 0.35f;
        Rect textClip = w.rect.Inset(2.f, 0.f);
        ctx.DrawText(Vec2(c.x - ts.x * 0.5f, textTop), label, th.textOnAccent, fs, &textClip);
    }
    ctx.EndWidget();
    return w.clicked;
}

// ---------------------------------------------------------------------------
void Toggle(Context& ctx, const char* label, bool* value) {
    const Theme& th = ctx.theme();
    WidgetFrame w = ctx.BeginWidget(label, th.smallHeight);
    WidgetState& st = *w.state;

    st.valueTarget = *value ? 1.f : 0.f;
    st.valueT = ExpSmooth(st.valueT, st.valueTarget, w.dt, 18.f);

    if (w.clicked) *value = !*value;

    if (w.rect.Valid()) {
        const float fs = th.fontSize;
        const float rowH = th.smallHeight;
        const float capH = fs * 0.72f;
        const float textTop = w.rect.y + (rowH - capH) * 0.5f - (fs - capH) * 0.35f;
        Rect textClip(w.rect.x, w.rect.y, w.rect.w - 48.f, w.rect.h);
        ctx.DrawText(Vec2(w.rect.x, textTop), label, th.text, fs, &textClip);

        // Track (right aligned pill)
        const float trackW = 40.f;
        const float trackH = rowH - 8.f;
        Rect track(w.rect.Max().x - trackW, w.rect.y + (rowH - trackH) * 0.5f, trackW, trackH);
        RectStyle trackSt;
        trackSt.cornerRadii = Vec4(trackH * 0.5f);
        trackSt.fill = Color::Blend(th.trackBg, th.accent, st.valueT);
        trackSt.fillEnd = Color::Blend(th.trackBg, th.accentActive, st.valueT);
        trackSt.gradientDir = Vec2(1.f, 0.f);
        trackSt.shadowColor = th.accent;
        trackSt.shadowBlur = 10.f;
        trackSt.shadowOffset = Vec2(0.f, 2.f);
        trackSt.shadowOpacity = 0.5f * st.valueT;
        ctx.DrawRect(track, trackSt);

        // Knob (slides with eased value)
        const float knob = trackH - 4.f;
        const float x = Lerp(track.x + 2.f, track.Max().x - knob - 2.f, st.valueT);
        RectStyle knobSt;
        knobSt.fill = Color::White();
        knobSt.cornerRadii = Vec4(knob * 0.5f);
        knobSt.shadowOffset = Vec2(0.f, 1.f);
        knobSt.shadowBlur = 3.f;
        knobSt.shadowOpacity = 0.35f;
        ctx.DrawRect(Rect(x, track.y + 2.f, knob, knob), knobSt);
    }
    ctx.EndWidget();
}

// ---------------------------------------------------------------------------
void Slider(Context& ctx, const char* label, float* value,
            float minValue, float maxValue) {
    const Theme& th = ctx.theme();
    const float widgetH = 46.f;
    WidgetFrame w = ctx.BeginWidget(label, widgetH);

    if (w.rect.Valid()) {
        const float fs = th.fontSize;
        ctx.DrawText(Vec2(w.rect.x, w.rect.y + 2.f), label, th.text, fs, &w.rect);

        char valText[32];
        std::snprintf(valText, sizeof(valText), "%.2f", (double)*value);
        const Vec2 vs = ctx.MeasureText(valText, fs);
        ctx.DrawText(Vec2(w.rect.Max().x - vs.x, w.rect.y + 2.f), valText, th.textDim, fs, &w.rect);

        // Track
        const float trackH = 6.f;
        Rect track(w.rect.x + 2.f, w.rect.y + widgetH - trackH - 12.f, w.rect.w - 4.f, trackH);

        float frac = Clamp01((*value - minValue) / std::max(maxValue - minValue, 1e-6f));
        if (w.active) {
            // Dragging: map mouse x (prev-frame track coords are fine at 60fps)
            const Vec2 m = ctx.mouse();
            frac = Clamp01((m.x - track.x) / std::max(track.w, 1.f));
            *value = minValue + frac * (maxValue - minValue);
        }

        RectStyle trackSt;
        trackSt.cornerRadii = Vec4(trackH * 0.5f);
        trackSt.fill = th.trackBg;
        ctx.DrawRect(track, trackSt);

        // Filled portion (gradient + soft glow)
        RectStyle fillSt;
        fillSt.cornerRadii = Vec4(trackH * 0.5f);
        fillSt.fill = th.accentActive;
        fillSt.fillEnd = th.accent;
        fillSt.gradientDir = Vec2(1.f, 0.f);
        fillSt.shadowColor = th.accent;
        fillSt.shadowBlur = 8.f;
        fillSt.shadowOpacity = 0.45f;
        const float fillW = std::max(track.w * frac, trackH);
        ctx.DrawRect(Rect(track.x, track.y, fillW, track.h), fillSt);

        // Knob grows slightly on hover/drag
        const float grow = std::max(w.state->hoverT * 0.5f, w.active ? 1.f : 0.f);
        const float knob = 15.f + 3.f * grow;
        const float kx = track.x + track.w * frac - knob * 0.5f;
        const float ky = track.y + track.h * 0.5f - knob * 0.5f;
        RectStyle knobSt;
        knobSt.fill = Color::White();
        knobSt.cornerRadii = Vec4(knob * 0.5f);
        knobSt.borderColor = Color(th.accent.r, th.accent.g, th.accent.b, 140);
        knobSt.borderWidth = 1.5f;
        knobSt.shadowColor = th.accent;
        knobSt.shadowOffset = Vec2(0.f, 2.f);
        knobSt.shadowBlur = 6.f + 4.f * grow;
        knobSt.shadowOpacity = 0.4f + 0.3f * grow;
        ctx.DrawRect(Rect(kx, ky, knob, knob), knobSt);
    }
    ctx.EndWidget();
}

// ---------------------------------------------------------------------------
void Label(Context& ctx, const char* text, const Color& color, float fontSize) {
    const Theme& th = ctx.theme();
    const float fs = fontSize > 0.f ? fontSize : th.fontSize;
    WidgetFrame w = ctx.BeginWidget(text, fs * 1.35f);
    Color col = color.a == 0 ? th.text : color;
    if (w.rect.Valid()) {
        ctx.DrawText(w.rect.Pos(), text, col, fs, &w.rect);
    }
    ctx.EndWidget();
}

} // namespace ui