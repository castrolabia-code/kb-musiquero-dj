#include "ui/DrawPrimitives.h"

#include "ui/KbLookAndFeel.h"
#include "ui/Palette.h"

namespace kb::ui
{
    using juce::Colour;
    using juce::Graphics;
    using juce::Justification;
    using juce::Point;
    using juce::Rectangle;
    using namespace palette;

    namespace draw
    {
        const float kArcStart = -juce::MathConstants<float>::pi * 0.75f;
        const float kArcEnd   =  juce::MathConstants<float>::pi * 0.75f;

        void text (Graphics& g, const juce::String& s, Rectangle<float> r, float height,
                   Colour c, Justification j, bool bold)
        {
            // Letras grandes (pedido del usuario): +25 % sobre el tamaño de diseño.
            g.setColour (c);
            g.setFont (KbLookAndFeel::labelFont (height * 1.25f, bold));
            g.drawFittedText (s, r.toNearestInt().expanded (0, juce::roundToInt (height * 0.3f)), j, 1, 0.75f);
        }

        /** Perilla con arco de valor naranja (sección 6). value01 en [0, 1]. */
        void knob (Graphics& g, Point<float> c, float r, float value01, const juce::String& label, float textH, bool bipolar)
        {
            // Sombra
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillEllipse (c.x - r, c.y - r + r * 0.18f, r * 2.0f, r * 2.0f);

            // Pista del arco + valor
            const auto arcR = r * 1.32f;
            const auto strokeW = juce::jmax (1.0f, r * 0.2f);
            juce::Path track;
            track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, kArcStart, kArcEnd, true);
            g.setColour (slot);
            g.strokePath (track, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const auto angle = kArcStart + (kArcEnd - kArcStart) * juce::jlimit (0.0f, 1.0f, value01);
            // Bipolar (EQ, GAIN, FILTER): el arco nace en el centro (12 en punto).
            const auto from = bipolar ? 0.0f : kArcStart;
            juce::Path value;
            if (std::abs (angle - from) > 0.01f)
                value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
            g.setColour (orange);
            g.strokePath (value, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Cuerpo con volumen
            const auto body = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
            g.setGradientFill (juce::ColourGradient (panelEdge.brighter (0.35f), c.x, c.y - r,
                                                     panelDark, c.x, c.y + r, false));
            g.fillEllipse (body);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawEllipse (body.reduced (0.5f), 1.0f);

            // Indicador
            g.setColour (textPrimary);
            g.drawLine (juce::Line<float> (c.getPointOnCircumference (r * 0.25f, angle),
                                           c.getPointOnCircumference (r * 0.82f, angle)),
                        juce::jmax (1.0f, r * 0.14f));

            text (g, label, Rectangle<float> (r * 5.0f, textH * 1.3f).withCentre ({ c.x, c.y + arcR + textH * 0.85f }),
                  textH, textSecondary);
        }

        /** Botón con glow. 'lit' = encendido. */
        void button (Graphics& g, Rectangle<float> r, Colour colour, const juce::String& label,
                     bool lit, bool round, float textH)
        {
            juce::Path shape;
            if (round)
                shape.addEllipse (r);
            else
                shape.addRoundedRectangle (r, r.getHeight() * 0.18f);

            if (lit)
                juce::DropShadow (colour.withAlpha (0.65f), juce::roundToInt (r.getHeight() * 0.45f), {}).drawForPath (g, shape);

            g.setColour (panelDark);
            g.fillPath (shape);
            g.setColour (lit ? colour : colour.withAlpha (0.22f));
            g.fillPath (shape, juce::AffineTransform::scale (0.9f, 0.9f, r.getCentreX(), r.getCentreY()));
            g.setColour (colour.withAlpha (lit ? 1.0f : 0.7f));
            g.strokePath (shape, juce::PathStrokeType (juce::jmax (1.0f, r.getHeight() * 0.05f)));

            text (g, label, r.reduced (r.getWidth() * 0.06f, 0.0f), textH, lit ? panelDark : colour);
        }

        void icon (Graphics& g, int id, Rectangle<float> a, Colour colour)
        {
            g.setColour (colour);
            const auto s = juce::jmin (a.getWidth(), a.getHeight());
            const auto sq = Rectangle<float> (s, s).withCentre (a.getCentre());
            juce::Path p;

            switch (id)
            {
                case 1: // PLAY: triángulo
                    p.addTriangle (sq.getX() + s * 0.18f, sq.getY(), sq.getX() + s * 0.18f, sq.getBottom(), sq.getRight() - s * 0.02f, sq.getCentreY());
                    g.fillPath (p);
                    break;

                case 2: // PAUSA: dos barras
                    g.fillRoundedRectangle (sq.getX() + s * 0.12f, sq.getY(), s * 0.28f, s, s * 0.06f);
                    g.fillRoundedRectangle (sq.getRight() - s * 0.40f, sq.getY(), s * 0.28f, s, s * 0.06f);
                    break;

                case 3: // CUE: barra + triángulo hacia atrás (volver al punto)
                    g.fillRoundedRectangle (sq.getX(), sq.getY(), s * 0.18f, s, s * 0.05f);
                    p.addTriangle (sq.getRight(), sq.getY(), sq.getRight(), sq.getBottom(), sq.getX() + s * 0.24f, sq.getCentreY());
                    g.fillPath (p);
                    break;

                case 4: // SYNC: dos flechas en círculo
                {
                    const auto c = sq.getCentre();
                    const auto r = s * 0.42f;
                    const auto w = juce::jmax (1.2f, s * 0.13f);
                    for (int k = 0; k < 2; ++k)
                    {
                        const auto start = juce::MathConstants<float>::pi * (float) k + 0.35f;
                        const auto end   = start + juce::MathConstants<float>::pi - 0.7f;
                        juce::Path arc;
                        arc.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
                        g.strokePath (arc, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                        // Punta de flecha al final del arco.
                        const auto tip = c.getPointOnCircumference (r, end);
                        const auto back1 = c.getPointOnCircumference (r + w * 1.4f, end - 0.45f);
                        const auto back2 = c.getPointOnCircumference (r - w * 1.4f, end - 0.45f);
                        juce::Path head;
                        head.addTriangle (tip.translated (0, 0), back1, back2);
                        g.fillPath (head);
                    }
                    break;
                }

                default:
                    break;
            }
        }

        void neonText (Graphics& g, const juce::String& text, Rectangle<float> area, Colour c, Justification j)
        {
            juce::GlyphArrangement ga;
            ga.addFittedText (KbLookAndFeel::displayFont (area.getHeight() * 0.86f), text,
                              area.getX(), area.getY(), area.getWidth(), area.getHeight(), j, 1, 0.9f);
            juce::Path p;
            ga.createPath (p);
            const auto h = area.getHeight();

            // Extrusión 3D (sombreado) hacia abajo-derecha.
            const auto depth = juce::jmax (2, juce::roundToInt (h * 0.1f));
            for (int k = depth; k >= 1; --k)
            {
                g.setColour (juce::Colour (0xff0a2a44).withAlpha (0.95f));
                g.fillPath (p, juce::AffineTransform::translation ((float) k * 0.6f, (float) k * 0.85f));
            }
            // Resplandor de tubo de neón.
            juce::DropShadow (c.withAlpha (0.75f), juce::roundToInt (h * 0.35f), {}).drawForPath (g, p);
            juce::DropShadow (c.withAlpha (0.9f), juce::roundToInt (h * 0.12f), {}).drawForPath (g, p);
            // Relleno: núcleo casi blanco arriba, color abajo.
            const auto pb = p.getBounds();
            g.setGradientFill (juce::ColourGradient (c.interpolatedWith (juce::Colours::white, 0.75f), 0.0f, pb.getY(),
                                                     c, 0.0f, pb.getBottom(), false));
            g.fillPath (p);
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.strokePath (p, juce::PathStrokeType (juce::jmax (0.8f, h * 0.022f)));
        }

        /** Ranura de fader con su tapa. pos01: 0 = abajo/izquierda, 1 = arriba/derecha. */
        void fader (Graphics& g, Rectangle<float> r, bool vertical, float pos01)
        {
            const auto slotR = vertical ? r.withSizeKeepingCentre (juce::jmax (3.0f, r.getWidth() * 0.18f), r.getHeight())
                                        : r.withSizeKeepingCentre (r.getWidth(), juce::jmax (3.0f, r.getHeight() * 0.18f));
            g.setColour (slot);
            g.fillRoundedRectangle (slotR, juce::jmin (slotR.getWidth(), slotR.getHeight()) * 0.5f);

            // Marcas de escala
            g.setColour (textSecondary.withAlpha (0.5f));
            for (int i = 0; i <= 10; ++i)
            {
                const auto t = (float) i / 10.0f;
                if (vertical)
                {
                    const auto y = r.getY() + r.getHeight() * t;
                    g.drawHorizontalLine ((int) y, r.getX(), r.getX() + r.getWidth() * 0.22f);
                }
                else
                {
                    const auto x = r.getX() + r.getWidth() * t;
                    g.drawVerticalLine ((int) x, r.getY(), r.getY() + r.getHeight() * 0.22f);
                }
            }

            // Tapa
            const auto capLong  = vertical ? r.getWidth() * 0.9f : r.getHeight() * 0.9f;
            const auto capShort = capLong * 0.45f;
            const auto capCentre = vertical ? Point<float> (r.getCentreX(), r.getBottom() - r.getHeight() * pos01)
                                            : Point<float> (r.getX() + r.getWidth() * pos01, r.getCentreY());
            const auto cap = vertical ? Rectangle<float> (capLong, capShort).withCentre (capCentre)
                                      : Rectangle<float> (capShort, capLong).withCentre (capCentre);
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), juce::roundToInt (capShort * 0.5f), { 0, 2 })
                .drawForRectangle (g, cap.toNearestInt());
            g.setGradientFill (juce::ColourGradient (panelEdge.brighter (0.5f), cap.getX(), cap.getY(),
                                                     panelDark, cap.getX(), cap.getBottom(), false));
            g.fillRoundedRectangle (cap, capShort * 0.2f);
            g.setColour (textPrimary);
            if (vertical)
                g.drawHorizontalLine ((int) cap.getCentreY(), cap.getX() + 2.0f, cap.getRight() - 2.0f);
            else
                g.drawVerticalLine ((int) cap.getCentreX(), cap.getY() + 2.0f, cap.getBottom() - 2.0f);
        }

        /** Color de un segmento LED según su posición en la escala (0 = abajo). */
        Colour meterColour (float t)
        {
            if (t < 0.60f) return meterGreen;
            if (t < 0.80f) return meterYellow;
            if (t < 0.92f) return meterOrange;
            return meterRed;
        }

        /** Columna/fila de LEDs apagados (los vúmetros reales llegan en la Fase 2). */
        void meter (Graphics& g, Rectangle<float> r, bool vertical, int segments)
        {
            g.setColour (slot);
            g.fillRoundedRectangle (r, 2.0f);
            const auto inner = r.reduced (juce::jmax (1.0f, juce::jmin (r.getWidth(), r.getHeight()) * 0.12f));
            const auto gapFrac = 0.25f;
            for (int i = 0; i < segments; ++i)
            {
                const auto t = (float) i / (float) segments;
                Rectangle<float> seg;
                if (vertical)
                {
                    const auto h = inner.getHeight() / (float) segments;
                    seg = { inner.getX(), inner.getBottom() - h * (float) (i + 1), inner.getWidth(), h * (1.0f - gapFrac) };
                }
                else
                {
                    const auto w = inner.getWidth() / (float) segments;
                    seg = { inner.getX() + w * (float) i, inner.getY(), w * (1.0f - gapFrac), inner.getHeight() };
                }
                g.setColour (meterColour (t).withAlpha (0.16f));
                g.fillRect (seg);
            }
        }
    } // namespace draw
} // namespace kb::ui
