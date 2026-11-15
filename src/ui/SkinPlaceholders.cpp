#include "ui/SkinPlaceholders.h"

#include "app/I18n.h"
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

    // =========================================================================
    //  Barra de estado
    // =========================================================================
    StatusBar::StatusBar() { setOpaque (false); }

    void StatusBar::setText (const juce::String& leftText, const juce::String& rightText)
    {
        if (leftText == left && rightText == right)
            return;
        left  = leftText;
        right = rightText;
        repaint();
    }

    void StatusBar::paint (Graphics& g)
    {
        // Pie sobre el chasis amarillo: estado a la izquierda, nombre al centro, versión a la derecha.
        auto b = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
        const auto h = b.getHeight();
        const auto ink = juce::Colour (0xff2a2400);
        const auto third = b.getWidth() / 3.0f;
        draw::text (g, left, b.removeFromLeft (third), h * 0.36f, ink.withAlpha (0.85f), Justification::centredLeft, false);
        auto rightArea = b.removeFromRight (third);
        draw::text (g, right, rightArea, h * 0.36f, ink.withAlpha (0.85f), Justification::centredRight, false);

        juce::GlyphArrangement ga;
        ga.addFittedText (KbLookAndFeel::displayFont (h * 0.62f), tr ("app.logo"), b.getX(), b.getY(), b.getWidth(), b.getHeight(),
                          Justification::centred, 1);
        juce::Path p;
        ga.createPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillPath (p, juce::AffineTransform::translation (0.0f, 1.2f));
        g.setColour (juce::Colours::black);
        g.fillPath (p);
    }
} // namespace kb::ui
