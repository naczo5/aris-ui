#include "ui/widgets/kit.hpp"

#include <algorithm>

namespace ui::kit {

namespace {

Color Mul(const Color& c, float a) {
    return Color(c.r, c.g, c.b, (std::uint8_t)std::clamp(c.a * a + 0.5f, 0.f, 255.f));
}

float TextTop(Context& ctx, float centerY, float fontSize) {
    return centerY - ctx.MeasureText("Ag", fontSize).y * 0.5f;
}

} // namespace

const Palette& Aurora() {
    static const Palette p = {
        /*panelTop*/        Color::Hex(0x130C1E, 235),
        /*panelBottom*/     Color::Hex(0x0E0718, 245),
        /*border*/          Color(255, 255, 255, 22),
        /*borderHighlight*/ Color(255, 255, 255, 55),
        /*divider*/         Color(255, 255, 255, 14),
        /*text*/            Color::Hex(0xFFFFFF),
        /*textDim*/         Color::Hex(0xB6AFC7),
        /*textFaint*/       Color::Hex(0x9288A6),
        /*accent*/          Color::Hex(0xAA3BFF),
        /*accentEnd*/       Color::Hex(0x7F22EC),
        /*accentGlow*/      Color::Hex(0x9E32FF),
        /*hover*/           Color(255, 255, 255, 14),
        /*track*/           Color(255, 255, 255, 28),
        /*blurRadius*/      35.f,
    };
    return p;
}

void GlassPanel(Context& ctx, const Rect& rect, const Palette& pal, float radius) {
    // 1) ambient accent glow behind the window
    RectStyle glow;
    glow.fill = Color::Transparent();
    glow.cornerRadii = Vec4(radius);
    glow.shadowColor = pal.accentGlow;
    glow.shadowOffset = Vec2(0.f, 10.f);
    glow.shadowBlur = 70.f;
    glow.shadowOpacity = 0.28f;
    ctx.DrawRect(rect, glow);

    // 1b) frosted backdrop: blur whatever the host drew behind the window
    if (pal.blurRadius > 0.f)
        ctx.DrawBackdropBlur(rect, Vec4(radius), pal.blurRadius, Color(18, 8, 36, 40));

    // 2) surface: gradient fill, hairline border, deep drop shadow
    RectStyle st;
    st.fill = pal.panelTop;
    st.fillEnd = pal.panelBottom;
    st.gradientDir = Vec2(0.25f, 1.f);
    st.cornerRadii = Vec4(radius);
    st.borderColor = pal.border;
    st.borderWidth = 1.f;
    st.shadowOffset = Vec2(0.f, 26.f);
    st.shadowBlur = 54.f;
    st.shadowOpacity = 0.6f;
    ctx.DrawRect(rect, st);

    // 3) top-edge sheen (fades out toward the corners)
    const float x0 = rect.x + radius, x1 = rect.Max().x - radius, mid = (x0 + x1) * 0.5f;
    RectStyle l, r;
    l.fill = Color(pal.borderHighlight.r, pal.borderHighlight.g, pal.borderHighlight.b, 0);
    l.fillEnd = pal.borderHighlight;
    l.gradientDir = Vec2(1.f, 0.f);
    r.fill = pal.borderHighlight;
    r.fillEnd = l.fill;
    r.gradientDir = Vec2(1.f, 0.f);
    ctx.DrawRect(Rect(x0, rect.y + 0.5f, mid - x0, 1.f), l);
    ctx.DrawRect(Rect(mid, rect.y + 0.5f, x1 - mid, 1.f), r);
}

bool NavItem(Context& ctx, const Palette& pal, const char* id, const Rect& rect,
             Icon icon, const char* label, bool selected) {
    WidgetFrame w = ctx.Interact(id, rect);
    WidgetState& st = *w.state;
    st.valueT = ExpSmooth(st.valueT, selected ? 1.f : 0.f, w.dt, 16.f);
    const float sel = st.valueT, hov = w.state->hoverT, prs = w.state->pressT;

    Rect r = rect;
    r = r.Inset(prs * 0.75f, prs * 0.75f);

    if (hov * (1.f - sel) > 0.01f) {
        RectStyle h;
        h.fill = Mul(pal.hover, hov * (1.f - sel));
        h.cornerRadii = Vec4(11.f);
        ctx.DrawRect(r, h);
    }
    if (sel > 0.01f) {
        RectStyle pill;
        pill.fill = Mul(pal.accent, sel);
        pill.fillEnd = Mul(pal.accentEnd, sel);
        pill.gradientDir = Vec2(1.f, 0.5f);
        pill.cornerRadii = Vec4(11.f);
        pill.borderColor = Color(255, 255, 255, (std::uint8_t)(48 * sel));
        pill.borderWidth = 1.f;
        pill.shadowColor = pal.accentGlow;
        pill.shadowOffset = Vec2(0.f, 5.f);
        pill.shadowBlur = 16.f;
        pill.shadowOpacity = 0.55f * sel;
        ctx.DrawRect(r, pill);
    }

    const Color idle = Color::Blend(pal.textDim, pal.text, hov * 0.6f);
    const Color col = Color::Blend(idle, Color::White(), sel);
    const float cy = r.y + r.h * 0.5f;
    DrawIcon(ctx, icon, Rect(r.x + 13.f, cy - 9.f, 18.f, 18.f), col, 1.75f);
    ctx.DrawText(Vec2(r.x + 41.f, TextTop(ctx, cy, 13.f)), label, col, 13.f, nullptr,
                 0.25f + 0.2f * sel);
    return w.clicked;
}

void SwitchGraphic(Context& ctx, const Palette& pal, const Rect& rect, float t) {
    RectStyle tr;
    tr.cornerRadii = Vec4(rect.h * 0.5f);
    tr.fill = Color::Blend(pal.track, pal.accent, t);
    tr.fillEnd = Color::Blend(pal.track, pal.accentEnd, t);
    tr.gradientDir = Vec2(1.f, 0.f);
    tr.shadowColor = pal.accentGlow;
    tr.shadowBlur = 10.f;
    tr.shadowOffset = Vec2(0.f, 2.f);
    tr.shadowOpacity = 0.5f * t;
    ctx.DrawRect(rect, tr);

    const float k = rect.h - 4.f;
    const float x = Lerp(rect.x + 2.f, rect.Max().x - k - 2.f, t);
    RectStyle knob;
    knob.fill = Color::White();
    knob.cornerRadii = Vec4(k * 0.5f);
    knob.shadowOffset = Vec2(0.f, 1.f);
    knob.shadowBlur = 3.f;
    knob.shadowOpacity = 0.35f;
    ctx.DrawRect(Rect(x, rect.y + 2.f, k, k), knob);
}

bool ModuleRow(Context& ctx, const Palette& pal, const char* id, const Rect& rect,
               const char* name, const char* description, bool* enabled, bool withSwitch) {
    WidgetFrame w = ctx.Interact(id, rect);
    WidgetState& st = *w.state;
    st.valueT = ExpSmooth(st.valueT, *enabled ? 1.f : 0.f, w.dt, 16.f);
    const float on = st.valueT, hov = st.hoverT, prs = st.pressT;

    Rect r = rect.Inset(prs * 0.5f, prs * 0.5f);
    if (hov > 0.01f) {
        RectStyle bg;
        bg.cornerRadii = Vec4(8.f);
        bg.fill = Color(255, 255, 255, (std::uint8_t)(15 * hov));
        bg.borderColor = Color(255, 255, 255, (std::uint8_t)(18 * hov));
        bg.borderWidth = 1.f;
        ctx.DrawRect(r, bg);
    }

    const float padX = 12.f;
    const float cy = r.y + r.h * 0.5f;
    const float h1 = ctx.MeasureText("Ag", 14.5f).y, h2 = ctx.MeasureText("Ag", 11.5f).y;
    const float top = cy - (h1 + 3.f + h2) * 0.5f;
    const Rect clip(r.x, r.y, withSwitch ? r.w - 52.f : r.w - 8.f, r.h);

    const Color nameCol = Color::Blend(Color::Hex(0xEDE7F5), Color::White(), on * 0.4f + hov * 0.6f);
    ctx.DrawText(Vec2(r.x + padX, top), name, nameCol, 14.5f, &clip, 0.4f);
    ctx.DrawText(Vec2(r.x + padX, top + h1 + 3.f), description,
                 pal.textFaint, 11.5f, &clip, -0.05f);

    if (withSwitch) {
        SwitchGraphic(ctx, pal, Rect(r.Max().x - padX - 32.f, cy - 9.f, 32.f, 18.f), on);
    }

    if (w.clicked) *enabled = !*enabled;
    return w.clicked;
}

bool IconButton(Context& ctx, const Palette& pal, const char* id, const Rect& rect, Icon icon) {
    WidgetFrame w = ctx.Interact(id, rect);
    const float hov = w.state->hoverT, prs = w.state->pressT;
    RectStyle bg;
    bg.cornerRadii = Vec4(rect.h * 0.5f);
    bg.fill = Mul(pal.hover, hov + prs * 0.5f);
    bg.borderColor = Mul(pal.border, hov);
    bg.borderWidth = 1.f;
    ctx.DrawRect(rect, bg);
    DrawIcon(ctx, icon, rect.Inset(rect.w * 0.28f, rect.h * 0.28f),
             Color::Blend(pal.textDim, pal.text, hov), 1.8f);
    return w.clicked;
}

void SectionTitle(Context& ctx, const Palette& pal, Vec2 pos, const char* text) {
    ctx.DrawText(pos, text, pal.textFaint, 10.f, nullptr, 0.35f);
}

void Avatar(Context& ctx, const Palette& pal, Vec2 c, float radius, const char* initial) {
    RectStyle ring;
    ring.cornerRadii = Vec4(radius);
    ring.fill = Color(255, 255, 255, 14);
    ring.fillEnd = Color(255, 255, 255, 4);
    ring.gradientDir = Vec2(0.f, 1.f);
    ring.borderColor = pal.border;
    ring.borderWidth = 1.f;
    ctx.DrawRect(Rect(c.x - radius, c.y - radius, radius * 2.f, radius * 2.f), ring);
    const Vec2 ts = ctx.MeasureText(initial, radius * 0.95f);
    ctx.DrawText(Vec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), initial, pal.text, radius * 0.95f,
                 nullptr, 0.3f);
}

} // namespace ui::kit
