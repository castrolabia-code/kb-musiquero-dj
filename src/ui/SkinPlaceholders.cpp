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
    StatusBar::StatusBar() { setOpaque (true); }

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
        g.fillAll (slot);
        g.setColour (panelEdge);
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());

        auto b = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
        const auto h = b.getHeight() * 0.58f;
        draw::text (g, right, b.removeFromRight (b.getWidth() * 0.2f), h, textSecondary, Justification::centredRight, false);
        draw::text (g, left, b, h, textSecondary, Justification::centredLeft, false);
    }
} // namespace kb::ui
