#include "ui/PlatterView.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/Palette.h"

#include <cmath>
#include <utility>

namespace kb::ui
{
    using namespace palette;

    PlatterView::PlatterView (int d) : deck (d)
    {
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    double PlatterView::angleForPosition (double seconds) noexcept
    {
        const auto turns = seconds * kRevolutionsPerSecond;
        return (turns - std::floor (turns)) * juce::MathConstants<double>::twoPi;
    }

    juce::String PlatterView::getTooltip() { return tr ("tip.platter"); }

    bool PlatterView::hitTest (int x, int y)
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        return c.getDistanceFrom ({ (float) x, (float) y }) <= getWidth() * 0.5f;
    }

    double PlatterView::angleAt (juce::Point<float> p) const
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        return std::atan2 ((double) (p.x - c.x), (double) (c.y - p.y));   // 0 arriba, + horario
    }

    void PlatterView::mouseDown (const juce::MouseEvent& e)
    {
        grabbed = true;
        lastAngle = angleAt (e.position);
        if (onTouch) onTouch (true);
    }

    void PlatterView::mouseDrag (const juce::MouseEvent& e)
    {
        if (! grabbed)
            return;
        const auto a = angleAt (e.position);
        const auto delta = std::remainder (a - lastAngle, juce::MathConstants<double>::twoPi);
        lastAngle = a;
        // Muy cerca del centro el ángulo salta: se ignora.
        if (e.position.getDistanceFrom (getLocalBounds().toFloat().getCentre()) < getWidth() * 0.08f)
            return;
        if (onTurn) onTurn (delta / juce::MathConstants<double>::twoPi);
    }

    void PlatterView::mouseUp (const juce::MouseEvent&)
    {
        if (! grabbed)
            return;
        grabbed = false;
        if (onTouch) onTouch (false);
    }

    void PlatterView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
    {
        if (onWheel) onWheel (w.deltaY);
    }

    void PlatterView::setArtwork (const juce::Image& image)
    {
        if (image.isValid() == artwork.isValid() && (! image.isValid() || image == artwork))
            return;
        artwork = image;
        repaint();
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
        // Vinilo negro con surcos y un brillo fijo (la luz no gira con el disco) y la
        // etiqueta central con anillos naranjas iluminados, como la referencia.
        const auto platter = getLocalBounds().toFloat();
        const auto d = platter.getWidth();
        const auto c = platter.getCentre();
        const auto vinyl = platter.reduced (d * 0.055f);
        const auto vr = vinyl.getWidth() * 0.5f;

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1c1f), c.x, c.y, juce::Colour (0xff060607), c.x + vr, c.y, true));
        g.fillEllipse (vinyl);

        // Surcos.
        for (float k = 0.4f; k < 0.97f; k += 0.035f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.03f + 0.02f * std::sin (k * 40.0f)));
            g.drawEllipse (vinyl.withSizeKeepingCentre (vinyl.getWidth() * k, vinyl.getHeight() * k), 1.0f);
        }

        // Brillos (reflejo de luz arriba-izquierda y abajo-derecha).
        for (auto [from, to] : { std::pair (-2.35f, -1.45f), std::pair (0.8f, 1.7f) })
        {
            juce::Path sheen;
            sheen.addPieSegment (vinyl.reduced (vr * 0.1f), from, to, 0.55f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), c.x, c.y,
                                                     juce::Colours::white.withAlpha (0.0f), c.x + vr, c.y, true));
            g.fillPath (sheen);
        }

        // Marca que gira (muestra la rotación y la posición).
        const auto rot = juce::AffineTransform::rotation ((float) angle, c.x, c.y);
        juce::Path marker;
        marker.addRoundedRectangle (c.x - d * 0.006f, vinyl.getY() + d * 0.02f, d * 0.012f, vr * 0.32f, d * 0.006f);
        g.setColour (juce::Colours::white.withAlpha (loaded ? 0.75f : 0.35f));
        g.fillPath (marker, rot);

        // Etiqueta: la CARÁTULA de la canción (gira con el disco) con un aro naranja
        // iluminado; sin carátula, anillos naranjas iluminados.
        const auto glow = playing ? 1.0f : (loaded ? 0.55f : 0.3f);
        const auto lr = artwork.isValid() ? vr * 0.6f : vr * 0.36f;
        const auto ring = [&] (float radius, float width, float alpha)
        {
            g.setColour (orange.withAlpha (alpha));
            g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c), width);
        };
        juce::Path labelPath;
        labelPath.addEllipse (juce::Rectangle<float> (lr * 2.0f, lr * 2.0f).withCentre (c));
        juce::DropShadow (orange.withAlpha (0.75f * glow), juce::roundToInt (lr * (artwork.isValid() ? 0.3f : 0.55f)), {}).drawForPath (g, labelPath);
        g.setColour (juce::Colour (0xff0b0b0c));
        g.fillPath (labelPath);

        if (artwork.isValid())
        {
            {
                juce::Graphics::ScopedSaveState save (g);
                g.reduceClipRegion (labelPath);
                const auto sc = (lr * 2.0f) / (float) juce::jmin (artwork.getWidth(), artwork.getHeight());
                const auto t = juce::AffineTransform::translation (-artwork.getWidth() * 0.5f, -artwork.getHeight() * 0.5f)
                                   .scaled (sc)
                                   .rotated ((float) angle)
                                   .translated (c);
                g.setOpacity (loaded ? 1.0f : 0.6f);
                g.drawImageTransformed (artwork, t);
            }
            ring (lr, juce::jmax (2.0f, lr * 0.05f), glow);
            // Agujero central (como un disco de verdad).
            g.setColour (juce::Colour (0xff0b0b0c));
            g.fillEllipse (juce::Rectangle<float> (lr * 0.16f, lr * 0.16f).withCentre (c));
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawEllipse (juce::Rectangle<float> (lr * 0.16f, lr * 0.16f).withCentre (c), 1.0f);
            return;
        }

        ring (lr * 0.92f, lr * 0.16f, glow);
        ring (lr * 0.92f, lr * 0.32f, 0.25f * glow);
        ring (lr * 0.58f, lr * 0.2f, glow);
        g.setColour (orange.withAlpha (glow).brighter (playing ? 0.3f : 0.0f));
        g.fillEllipse (juce::Rectangle<float> (lr * 0.6f, lr * 0.6f).withCentre (c));
        g.setColour (juce::Colour (0xff0b0b0c));
        g.fillEllipse (juce::Rectangle<float> (lr * 0.22f, lr * 0.22f).withCentre (c));
        // Muesca en la etiqueta (también gira).
        juce::Path notch;
        notch.addEllipse (c.x - lr * 0.07f, c.y - lr * 0.82f, lr * 0.14f, lr * 0.14f);
        g.setColour (juce::Colour (0xff0b0b0c));
        g.fillPath (notch, rot);
    }

} // namespace kb::ui
