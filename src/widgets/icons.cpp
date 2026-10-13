#include "ui/widgets/icons.hpp"

#include <cmath>
#include <initializer_list>
#include <utility>

namespace ui {

namespace {

// Tiny turtle that maps a 24x24 design grid onto a screen box.
struct Pen {
    Context& ctx;
    Vec2 origin;
    float s;          // grid unit -> px
    Color color;
    float stroke;     // px

    Vec2 P(float x, float y) const { return Vec2(origin.x + x * s, origin.y + y * s); }

    void L(float x1, float y1, float x2, float y2, float w = 1.f) {
        ctx.DrawLine(P(x1, y1), P(x2, y2), stroke * w, color);
    }
    void Poly(std::initializer_list<std::pair<float, float>> pts, float w = 1.f) {
        const std::pair<float, float>* prev = nullptr;
        for (const auto& p : pts) {
            if (prev) L(prev->first, prev->second, p.first, p.second, w);
            prev = &p;
        }
    }
    void Ring(float cx, float cy, float r, float w = 1.f) {
        ctx.DrawCircle(P(cx, cy), r * s, color, stroke * w);
    }
    void Dot(float cx, float cy, float r) { ctx.DrawCircle(P(cx, cy), r * s, color); }

    // Arc from a0..a1 degrees (y-down: 0 = right, 90 = down, 270 = up).
    void Arc(float cx, float cy, float r, float a0, float a1, int steps = 14) {
        const float k = 3.14159265f / 180.f;
        float px = cx + r * std::cos(a0 * k), py = cy + r * std::sin(a0 * k);
        for (int i = 1; i <= steps; ++i) {
            const float a = (a0 + (a1 - a0) * (float)i / (float)steps) * k;
            const float x = cx + r * std::cos(a), y = cy + r * std::sin(a);
            L(px, py, x, y);
            px = x; py = y;
        }
    }
    // Quadratic bezier through control point c.
    void Quad(float x0, float y0, float cx, float cy, float x1, float y1, int steps = 14) {
        float px = x0, py = y0;
        for (int i = 1; i <= steps; ++i) {
            const float t = (float)i / (float)steps, u = 1.f - t;
            const float x = u * u * x0 + 2 * u * t * cx + t * t * x1;
            const float y = u * u * y0 + 2 * u * t * cy + t * t * y1;
            L(px, py, x, y);
            px = x; py = y;
        }
    }
};

} // namespace

void DrawIcon(Context& ctx, Icon icon, const Rect& box, const Color& color, float stroke) {
    const float size = std::min(box.w, box.h);
    const float s = size / 24.f;
    Pen p{ctx, Vec2(box.x + (box.w - size) * 0.5f, box.y + (box.h - size) * 0.5f), s, color,
          std::max(stroke * s, 1.f)};

    switch (icon) {
    case Icon::Combat:
        // Grim-style combat icon: bow & arrow notched diagonally
        p.Quad(6.5f, 5.5f, 3.2f, 12.f, 6.5f, 18.5f, 12);  // bow stave
        p.L(6.5f, 5.5f, 10.5f, 12.f, 0.85f);              // top string
        p.L(6.5f, 18.5f, 10.5f, 12.f, 0.85f);             // bottom string
        p.L(5.f, 17.5f, 19.5f, 4.5f, 1.25f);              // arrow shaft
        p.Poly({{14.f, 4.5f}, {19.5f, 4.5f}, {19.5f, 10.f}}, 1.2f); // arrowhead
        p.L(4.2f, 15.8f, 6.8f, 18.4f, 1.f);               // fletching notch
        break;
    case Icon::Movement:
        p.Dot(14.8f, 4.6f, 2.1f);
        p.Poly({{13.2f, 8.6f}, {10.4f, 13.6f}});
        p.Poly({{13.0f, 9.2f}, {17.4f, 11.6f}, {19.8f, 10.4f}});
        p.Poly({{12.2f, 10.2f}, {8.2f, 10.2f}, {6.0f, 13.0f}});
        p.Poly({{10.4f, 13.6f}, {14.2f, 16.8f}, {13.2f, 21.0f}});
        p.Poly({{10.4f, 13.6f}, {8.0f, 17.6f}, {4.4f, 18.8f}});
        break;
    case Icon::Player:
        p.Ring(12.f, 7.8f, 3.6f);
        p.Arc(12.f, 21.f, 8.4f, 205.f, 335.f, 12);
        break;
    case Icon::Visuals:
        p.Quad(2.2f, 12.f, 12.f, 3.2f, 21.8f, 12.f, 16);
        p.Quad(2.2f, 12.f, 12.f, 20.8f, 21.8f, 12.f, 16);
        p.Ring(12.f, 12.f, 3.4f);
        p.Dot(12.f, 12.f, 1.2f);
        break;
    case Icon::Settings: {
        p.Ring(12.f, 12.f, 3.2f);
        p.Ring(12.f, 12.f, 7.4f, 1.f);
        for (int i = 0; i < 8; ++i) {
            const float a = (float)i * 0.785398f;
            p.L(12.f + 7.4f * std::cos(a), 12.f + 7.4f * std::sin(a),
                12.f + 9.9f * std::cos(a), 12.f + 9.9f * std::sin(a), 1.7f);
        }
        break;
    }
    case Icon::Friends:
        p.Ring(9.f, 8.f, 3.2f);
        p.Arc(9.f, 20.f, 6.6f, 205.f, 335.f, 10);
        p.Ring(17.2f, 9.2f, 2.5f);
        p.Arc(17.4f, 19.2f, 4.9f, 218.f, 335.f, 9);
        break;
    case Icon::Configs:
        p.Poly({{3.5f, 6.5f}, {3.5f, 19.f}, {20.5f, 19.f}, {20.5f, 9.5f}, {11.f, 9.5f},
                {9.f, 6.5f}, {3.5f, 6.5f}});
        break;
    case Icon::Hud:
        p.Poly({{3.5f, 5.f}, {20.5f, 5.f}, {20.5f, 16.f}, {3.5f, 16.f}, {3.5f, 5.f}});
        p.L(12.f, 16.f, 12.f, 19.5f);
        p.L(8.f, 19.5f, 16.f, 19.5f);
        break;
    case Icon::Search:
        p.Ring(10.6f, 10.6f, 6.6f);
        p.L(15.6f, 15.6f, 20.6f, 20.6f);
        break;
    case Icon::Check:
        p.Poly({{4.5f, 12.8f}, {9.6f, 17.6f}, {19.6f, 6.6f}}, 1.15f);
        break;
    case Icon::ChevronRight:
        p.Poly({{9.f, 5.f}, {16.f, 12.f}, {9.f, 19.f}});
        break;
    case Icon::ChevronDown:
        p.Poly({{5.f, 9.f}, {12.f, 16.f}, {19.f, 9.f}});
        break;
    case Icon::Close:
        p.L(6.f, 6.f, 18.f, 18.f);
        p.L(18.f, 6.f, 6.f, 18.f);
        break;
    case Icon::Plus:
        p.L(12.f, 5.f, 12.f, 19.f);
        p.L(5.f, 12.f, 19.f, 12.f);
        break;
    case Icon::Bolt:
        p.Poly({{13.5f, 2.8f}, {5.f, 13.6f}, {11.5f, 13.6f}, {10.2f, 21.2f}, {19.f, 9.8f},
                {12.6f, 9.8f}, {13.5f, 2.8f}});
        break;
    case Icon::Shield:
        p.Poly({{12.f, 3.f}, {19.5f, 6.f}, {19.5f, 12.f}});
        p.Quad(19.5f, 12.f, 19.f, 18.f, 12.f, 21.f, 10);
        p.Quad(12.f, 21.f, 5.f, 18.f, 4.5f, 12.f, 10);
        p.Poly({{4.5f, 12.f}, {4.5f, 6.f}, {12.f, 3.f}});
        break;
    case Icon::GrimCrest: {
        // Metallic heraldic shield with horns and knight/crest detail
        p.Poly({{2.2f, 2.2f}, {4.5f, 4.5f}, {19.5f, 4.5f}, {21.8f, 2.2f}}, 1.2f);
        p.L(2.2f, 2.2f, 3.8f, 7.5f, 1.0f);
        p.L(21.8f, 2.2f, 20.2f, 7.5f, 1.0f);
        // Outer shield contour
        p.Poly({{4.5f, 4.5f}, {19.5f, 4.5f}, {19.5f, 12.5f}});
        p.Quad(19.5f, 12.5f, 19.f, 18.5f, 12.f, 22.f, 10);
        p.Quad(12.f, 22.f, 5.f, 18.5f, 4.5f, 12.5f, 10);
        p.Poly({{4.5f, 12.5f}, {4.5f, 4.5f}});
        // inner emblem: vertical spine, visor cross, chevron
        p.L(12.f, 6.5f, 12.f, 17.5f, 1.2f);
        p.L(8.f, 9.8f, 16.f, 9.8f, 1.2f);
        p.Poly({{8.5f, 13.5f}, {12.f, 16.8f}, {15.5f, 13.5f}}, 1.1f);
        p.Poly({{9.5f, 7.2f}, {12.f, 9.2f}, {14.5f, 7.2f}}, 1.0f);
        break;
    }
    case Icon::Cursor: {
        // Solid-feel OS pointer arrow: fill strokes + outline
        for (float fx = 3.5f; fx <= 13.5f; fx += 1.f) {
            const float topY = 2.5f + (fx - 2.5f) * 0.85f;
            const float botY = 14.f - (fx - 3.5f) * 0.1f;
            if (botY > topY) ctx.DrawLine(p.P(fx, topY), p.P(fx, botY), stroke * 1.1f, color);
        }
        ctx.DrawLine(p.P(7.5f, 13.5f), p.P(11.5f, 20.f), stroke * 2.2f, color);
        p.Poly({{2.5f, 2.5f}, {2.5f, 19.f}, {6.8f, 14.8f}, {10.5f, 21.5f},
                {13.2f, 20.f}, {9.5f, 13.5f}, {15.5f, 13.5f}, {2.5f, 2.5f}}, 1.25f);
        break;
    }
    case Icon::Count: break;
    }
}

} // namespace ui
