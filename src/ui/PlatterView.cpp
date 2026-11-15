#include "ui/PlatterView.h"

#include "ui/DrawPrimitives.h"
#include "ui/Palette.h"

#include <cmath>

namespace kb::ui
{
    using namespace palette;

    PlatterView::PlatterView (int d) : deck (d)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);   // Fase 4: jog (scratch / bend)
    }

    double PlatterView::angleForPosition (double seconds) noexcept
    {
        const auto turns = seconds * kRevolutionsPerSecond;
        return (turns - std::floor (turns)) * juce::MathConstants<double>::twoPi;
    }

    void PlatterView::setState (double positionSeconds, bool isPlaying, bool isLoaded)
    {
        const auto a = angleForPosition (positionSeconds);
        // Medio grado de giro ya se nota; por debajo no vale la pena repintar.
        const auto moved = std::abs (std::remainder (a - angle, juce::MathConstants<double>::twoPi)) > 0.008;
        if (moved || isPlaying != playing || isLoaded != loaded)
        {
            angle = a;
            playing = isPlaying;
            loaded = isLoaded;
            repaint();
        }
    }

    void PlatterView::paint (juce::Graphics& g)
    {
        const auto platter = getLocalBounds().toFloat();
        const auto diameter = platter.getWidth();
        const auto c = platter.getCentre();
        const auto accent = deckColour (deck);
        const auto unit = diameter / 60.0f;

        // --- Anillo luminoso -------------------------------------------------------------
        const auto ringR = diameter * 0.455f;
        const auto ringW = juce::jmax (1.5f, diameter * 0.022f);
        if (playing)
        {
            // Glow barato: trazos concéntricos cada vez más transparentes.
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (orange.withAlpha (0.10f * (float) (4 - i)));
                g.drawEllipse (juce::Rectangle<float> (ringR * 2.0f, ringR * 2.0f).withCentre (c), ringW + unit * (float) i * 1.4f);
            }
        }
        g.setColour (playing ? orange : orange.withAlpha (loaded ? 0.55f : 0.35f));
        g.drawEllipse (juce::Rectangle<float> (ringR * 2.0f, ringR * 2.0f).withCentre (c), ringW);

        // --- Vinilo -----------------------------------------------------------------------
        const auto vinyl = platter.reduced (diameter * 0.075f);
        g.setGradientFill (juce::ColourGradient (panel, c.x, c.y, slot, vinyl.getRight(), c.y, true));
        g.fillEllipse (vinyl);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        for (float k = 0.36f; k < 0.98f; k += 0.055f)
            g.drawEllipse (vinyl.withSizeKeepingCentre (vinyl.getWidth() * k, vinyl.getHeight() * k), 1.0f);

        // --- Partes que giran ------------------------------------------------------------
        const auto rot = juce::AffineTransform::rotation ((float) angle, c.x, c.y);

        // Marca de posición (como la del plato de un CDJ).
        juce::Path marker;
        marker.addRoundedRectangle (c.x - unit * 0.45f, vinyl.getY() + unit, unit * 0.9f, vinyl.getHeight() * 0.17f, unit * 0.4f);
        g.setColour (playing ? textPrimary : textPrimary.withAlpha (0.8f));
        g.fillPath (marker, rot);

        // Etiqueta central con el número del deck y una marca de color.
        const auto label = vinyl.withSizeKeepingCentre (vinyl.getWidth() * 0.3f, vinyl.getHeight() * 0.3f);
        g.setColour (panelDark);
        g.fillEllipse (label);
        g.setColour (accent);
        g.drawEllipse (label, juce::jmax (1.0f, unit * 0.4f));

        juce::Path tick;
        tick.addCentredArc (c.x, c.y, label.getWidth() * 0.38f, label.getWidth() * 0.38f, 0.0f, -0.5f, 0.5f, true);
        g.strokePath (tick, juce::PathStrokeType (juce::jmax (1.5f, unit * 0.7f), juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded), rot);

        {
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (rot);
            draw::text (g, juce::String (deck + 1), label, label.getHeight() * 0.5f, accent);
        }

        // Reflejo sutil (fijo: la luz no gira con el disco).
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), vinyl.getX(), vinyl.getY(),
                                                 juce::Colours::transparentWhite, c.x, c.y, false));
        g.fillEllipse (vinyl);
    }
} // namespace kb::ui
