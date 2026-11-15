#include "ui/TribalArt.h"

#include <cmath>

namespace kb::ui::tribal
{
    using juce::Path;
    using juce::Point;
    using juce::Rectangle;

    static constexpr float kPi = juce::MathConstants<float>::pi;

    Path koru (Point<float> c, float radius, float turns, float start, float dir, float thickness)
    {
        // Borde exterior hacia adentro y borde interior de vuelta: forma cerrada que se afina.
        constexpr int steps = 72;
        Path p;
        juce::Array<Point<float>> outer, inner;
        for (int i = 0; i <= steps; ++i)
        {
            const auto t = (float) i / steps;
            const auto a = start + dir * turns * 2.0f * kPi * t;
            const auto r = radius * (1.0f - 0.82f * t);
            const auto w = thickness * (1.0f - 0.9f * t) + 0.4f;
            outer.add ({ c.x + r * std::sin (a), c.y - r * std::cos (a) });
            inner.add ({ c.x + (r - w) * std::sin (a), c.y - (r - w) * std::cos (a) });
        }
        p.startNewSubPath (outer[0]);
        for (int i = 1; i < outer.size(); ++i) p.lineTo (outer[i]);
        for (int i = inner.size() - 1; i >= 0; --i) p.lineTo (inner[i]);
        p.closeSubPath();
        return p;
    }

    Path thorn (Point<float> a, Point<float> b, Point<float> tip, float bend)
    {
        // Dos curvas que se juntan en la punta, combadas hacia el mismo lado.
        const auto mid = (a + b) * 0.5f;
        const auto axis = tip - mid;
        const Point<float> normal (-axis.y, axis.x);
        const auto len = std::sqrt (normal.x * normal.x + normal.y * normal.y);
        const auto n = len > 0.0f ? normal / len : Point<float>();
        const auto off = n * (bend * std::sqrt (axis.x * axis.x + axis.y * axis.y));

        Path p;
        p.startNewSubPath (a);
        p.quadraticTo ((a + tip) * 0.5f + off * 1.15f, tip);
        p.quadraticTo ((b + tip) * 0.5f + off * 0.75f, b);
        p.closeSubPath();
        return p;
    }

    Path band (Rectangle<float> r)
    {
        // Centro: rombo con espirales enfrentadas; a los lados, espinas que se
        // estiran hacia las puntas alternando arriba y abajo (simétrico).
        Path p;
        const auto c = r.getCentre();
        const auto h = r.getHeight();
        const auto half = r.getWidth() * 0.5f;

        p.addPath (thorn ({ c.x - h * 0.35f, c.y }, { c.x + h * 0.35f, c.y }, { c.x, r.getY() }, 0.0f));
        p.addPath (thorn ({ c.x - h * 0.35f, c.y }, { c.x + h * 0.35f, c.y }, { c.x, r.getBottom() }, 0.0f));

        for (int side = -1; side <= 1; side += 2)
        {
            const auto s = (float) side;
            p.addPath (koru ({ c.x + s * h * 0.75f, c.y }, h * 0.42f, 1.2f, s > 0 ? kPi : 0.0f, -s, h * 0.2f));

            const auto segments = juce::jmax (2, (int) (half / (h * 1.2f)));
            for (int k = 1; k <= segments; ++k)
            {
                const auto x0 = c.x + s * (h * 1.1f + (half - h * 1.3f) * (float) (k - 1) / (float) segments);
                const auto x1 = c.x + s * (h * 1.1f + (half - h * 1.3f) * (float) k / (float) segments);
                const auto up = (k % 2) == 0;
                const auto yBase = c.y + (up ? h * 0.12f : -h * 0.12f);
                const auto yTip  = up ? r.getY() + h * 0.08f : r.getBottom() - h * 0.08f;
                p.addPath (thorn ({ x0, yBase - h * 0.13f }, { x0, yBase + h * 0.13f }, { x1, yTip }, 0.25f * s * (up ? 1.0f : -1.0f)));
            }
            // Cola larga hasta la punta.
            p.addPath (thorn ({ c.x + s * h * 1.1f, c.y - h * 0.09f }, { c.x + s * h * 1.1f, c.y + h * 0.09f },
                              { c.x + s * (half - 1.0f), c.y }, 0.04f * s));
        }
        return p;
    }

    Path flame (Rectangle<float> r)
    {
        // Diseño vertical simétrico, estilo tatuaje tribal:
        //   * dos espirales enfrentadas arriba (forma de corazón),
        //   * "cuernos": cuchillas largas que salen de las espirales hacia arriba y afuera,
        //   * ganchos filosos a los costados que bajan curvándose,
        //   * una espina central larga hacia abajo.
        Path p;
        const auto w  = r.getWidth();
        const auto h  = r.getHeight();
        const auto cx = r.getCentreX();
        const auto y0 = r.getY();
        auto Y = [&] (float f) { return y0 + h * f; };

        for (int side = -1; side <= 1; side += 2)
        {
            const auto s = (float) side;
            // Espiral que se enrolla hacia afuera.
            p.addPath (koru ({ cx + s * w * 0.2f, Y (0.16f) }, w * 0.2f, 1.35f, s > 0 ? kPi * 1.5f : kPi * 0.5f, s, w * 0.09f));
            // Cuerno: de la base de la espiral hacia arriba y afuera.
            p.addPath (thorn ({ cx + s * w * 0.04f, Y (0.24f) }, { cx + s * w * 0.16f, Y (0.3f) },
                              { cx + s * w * 0.5f, y0 }, -0.28f * s));
            // Ganchos laterales (dos niveles), filosos y curvados hacia abajo.
            p.addPath (thorn ({ cx + s * w * 0.03f, Y (0.36f) }, { cx + s * w * 0.03f, Y (0.46f) },
                              { cx + s * w * 0.5f, Y (0.5f) }, 0.32f * s));
            p.addPath (thorn ({ cx + s * w * 0.03f, Y (0.56f) }, { cx + s * w * 0.03f, Y (0.64f) },
                              { cx + s * w * 0.4f, Y (0.74f) }, 0.3f * s));
            // Púas chicas abajo.
            p.addPath (thorn ({ cx + s * w * 0.02f, Y (0.74f) }, { cx + s * w * 0.02f, Y (0.8f) },
                              { cx + s * w * 0.24f, Y (0.9f) }, 0.25f * s));
        }
        // Rombo central y espina larga.
        p.addPath (thorn ({ cx - w * 0.11f, Y (0.34f) }, { cx + w * 0.11f, Y (0.34f) }, { cx, Y (0.2f) }, 0.0f));
        p.addPath (thorn ({ cx - w * 0.11f, Y (0.34f) }, { cx + w * 0.11f, Y (0.34f) }, { cx, r.getBottom() }, 0.0f));
        return p;
    }

    Path sweep (Rectangle<float> r, int seed, bool flip)
    {
        // Se compone en unidades del ANCHO (así las espirales quedan redondas) y
        // las posiciones verticales se reparten en el alto. Variaciones según 'seed'.
        auto rnd = [seed] (int k) { return 0.5f + 0.5f * std::sin ((float) seed * 12.9898f + (float) k * 78.233f); };
        const auto w = r.getWidth();
        const auto h = r.getHeight();
        auto P = [&] (float fx, float fy) { return Point<float> (fx * w, fy * h); };
        const auto a = rnd (1) * 0.1f, b = rnd (2) * 0.1f, c = rnd (3) * 0.08f;

        Path p;
        // Espiral grande arriba (enrollada hacia adentro) y una chica abajo, al revés.
        p.addPath (koru (P (0.42f + a, 0.17f + c), w * 0.3f, 2.0f, 4.0f + a * 6.0f, 1.0f, w * 0.075f));
        p.addPath (koru (P (0.62f - b, 0.8f), w * 0.18f, 1.8f, 1.0f + b * 6.0f, -1.0f, w * 0.05f));

        // Cuchilla principal: nace junto a la espiral y barre en diagonal hasta abajo.
        p.addPath (thorn (P (0.55f + a, 0.28f), P (0.7f + a, 0.33f), P (0.04f, 0.97f), 0.3f));
        // Cuchilla larga que sube hacia el otro lado.
        p.addPath (thorn (P (0.2f, 0.6f + c), P (0.34f, 0.64f + c), P (0.98f, 0.04f + b), -0.25f));
        // Ramas con púas.
        p.addPath (thorn (P (0.38f, 0.48f), P (0.46f, 0.52f), P (0.97f, 0.5f - c), 0.25f));
        p.addPath (thorn (P (0.22f, 0.76f), P (0.3f, 0.79f), P (0.02f, 0.56f + a), -0.32f));
        p.addPath (thorn (P (0.5f, 0.9f), P (0.58f, 0.92f), P (0.98f, 0.99f), 0.18f));
        // Ganchos sueltos (detalle grafiti).
        p.addPath (thorn (P (0.06f, 0.36f), P (0.14f, 0.4f), P (0.03f, 0.05f + b), 0.4f));
        p.addPath (thorn (P (0.86f, 0.34f + c), P (0.94f, 0.37f + c), P (0.66f, 0.62f), -0.4f));

        auto t = juce::AffineTransform();
        if (flip)
            t = t.scaled (-1.0f, 1.0f).translated (w, 0.0f);
        p.applyTransform (t.translated (r.getX(), r.getY()));
        return p;
    }

    Path corner (Rectangle<float> r, int quadrant)
    {
        // Se dibuja para la esquina superior izquierda y se espeja.
        Path p;
        const auto s = juce::jmin (r.getWidth(), r.getHeight());
        const Rectangle<float> box (0.0f, 0.0f, s, s);
        p.addPath (koru ({ s * 0.34f, s * 0.34f }, s * 0.28f, 1.5f, kPi * 1.25f, 1.0f, s * 0.1f));
        p.addPath (thorn ({ s * 0.5f, s * 0.12f }, { s * 0.5f, s * 0.26f }, { s * 1.0f, s * 0.08f }, -0.15f));
        p.addPath (thorn ({ s * 0.12f, s * 0.5f }, { s * 0.26f, s * 0.5f }, { s * 0.08f, s * 1.0f }, 0.15f));
        p.addPath (thorn ({ s * 0.46f, s * 0.42f }, { s * 0.42f, s * 0.46f }, { s * 0.8f, s * 0.8f }, 0.2f));

        auto t = juce::AffineTransform();
        if (quadrant == 1 || quadrant == 2) t = t.scaled (-1.0f, 1.0f, s * 0.5f, 0.0f);
        if (quadrant == 2 || quadrant == 3) t = t.scaled (1.0f, -1.0f, 0.0f, s * 0.5f);
        const auto x = (quadrant == 1 || quadrant == 2) ? r.getRight() - s : r.getX();
        const auto y = (quadrant == 2 || quadrant == 3) ? r.getBottom() - s : r.getY();
        p.applyTransform (t.translated (x, y));
        juce::ignoreUnused (box);
        return p;
    }

    Path texture (Rectangle<float> r, float cell)
    {
        // Trama irregular pero determinística: espirales y espinas alternadas.
        Path p;
        int i = 0;
        for (auto y = r.getY(); y < r.getBottom() + cell; y += cell * 0.9f, ++i)
        {
            int j = 0;
            for (auto x = r.getX() - ((i % 2) * cell * 0.5f); x < r.getRight() + cell; x += cell, ++j)
            {
                const auto k = (i * 7 + j * 13) % 5;
                const Point<float> c (x, y);
                if (k == 0 || k == 3)
                    p.addPath (koru (c, cell * 0.36f, 1.4f, (float) (i + j) * 0.9f, (k == 0) ? 1.0f : -1.0f, cell * 0.08f));
                else
                    p.addPath (thorn ({ c.x - cell * 0.3f, c.y - cell * 0.06f }, { c.x - cell * 0.3f, c.y + cell * 0.06f },
                                      { c.x + cell * 0.38f, c.y + (k == 1 ? -cell * 0.25f : cell * 0.25f) }, k == 1 ? 0.3f : -0.3f));
            }
        }
        return p;
    }
} // namespace kb::ui::tribal
