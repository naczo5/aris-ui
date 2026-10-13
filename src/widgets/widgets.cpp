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
        Color fill = Blend2(th.accent, th.accentHover, w.state->hoverT,
                            th.accentActive, w.state->pressT);
        RectStyle st;
        st.fill = fill;
        st.cornerRadii = Vec4(th.controlRadius);
        // Hover lifts the button; press settles it back down.
        st.shadowOffset = Vec2(0.f, Lerp(2.f, 5.f, w.state->hoverT));
        st.shadowBlur = 12.f;
        st.shadowOpacity = 0.18f + 0.14f * w.state->hoverT - 0.10f * w.state->pressT;
        ctx.DrawRect(w.rect, st);

        const float fs = th.fontSize;
        const Vec2 ts = ctx.MeasureText(label, fs);
        const Vec2 c = w.rect.Center();
        ctx.DrawText(Vec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), label, th.textOnAccent, fs, &w.rect);
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
        const Vec2 ts = ctx.MeasureText(label, fs);
        ctx.DrawText(Vec2(w.rect.x, w.rect.y + (rowH - ts.y) * 0.5f), label,
                     th.text, fs, &w.rect);

        // Track (right aligned pill)
        const float trackW = 40.f;
        const float trackH = rowH - 8.f;
        Rect track(w.rect.Max().x - trackW, w.rect.y + (rowH - trackH) * 0.5f, trackW, trackH);
        RectStyle trackSt;
        trackSt.cornerRadii = Vec4(trackH * 0.5f);
        trackSt.fill = Color::Blend(th.trackBg, th.accent, st.valueT);
        trackSt.borderColor = th.controlBorder;
        trackSt.borderWidth = 1.f;
        ctx.DrawRect(track, trackSt);

        // Knob (slides with eased value)
        const float knob = trackH - 4.f;
        const float x = Lerp(track.x + 2.f, track.Max().x - knob - 2.f, st.valueT);
        RectStyle knobSt;
        knobSt.fill = Color::White();
        knobSt.cornerRadii = Vec4(knob * 0.5f);
        knobSt.shadowOffset = Vec2(0.f, 1.f);
        knobSt.shadowBlur = 3.f;
        knobSt.shadowOpacity = 0.25f;
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

        // Filled portion
        RectStyle fillSt;
        fillSt.cornerRadii = Vec4(trackH * 0.5f);
        fillSt.fill = th.accent;
        const float fillW = std::max(track.w * frac, trackH);
        ctx.DrawRect(Rect(track.x, track.y, fillW, track.h), fillSt);

        // Knob
        const float knob = 16.f;
        const float kx = track.x + track.w * frac - knob * 0.5f;
        const float ky = track.y + track.h * 0.5f - knob * 0.5f;
        RectStyle knobSt;
        knobSt.fill = w.active ? th.text : Color::White();
        knobSt.cornerRadii = Vec4(knob * 0.5f);
        knobSt.shadowOffset = Vec2(0.f, 2.f);
        knobSt.shadowBlur = 4.f;
        knobSt.shadowOpacity = 0.25f;
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