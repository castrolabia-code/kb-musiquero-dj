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
            g.setColour (c);
            g.setFont (KbLookAndFeel::labelFont (height, bold));
            g.drawFittedText (s, r.toNearestInt(), j, 1, 0.8f);
        }

        /** Perilla con arco de valor naranja (sección 6). value01 en [0, 1]. */
        void knob (Graphics& g, Point<float> c, float r, float value01, const juce::String& label, float textH)
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
            juce::Path value;
            value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, kArcStart, angle, true);
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
