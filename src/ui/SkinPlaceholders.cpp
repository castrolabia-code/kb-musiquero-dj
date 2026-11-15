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
    //  Biblioteca
    // =========================================================================
    LibraryPlaceholder::LibraryPlaceholder() { setOpaque (true); }

    void LibraryPlaceholder::paint (Graphics& g)
    {
        g.fillAll (panelDark);

        auto b = getLocalBounds().toFloat().reduced (2.0f);
        const auto rowH = juce::jlimit (18.0f, 40.0f, b.getHeight() / 10.0f);
        const auto textH = rowH * 0.55f;

        // Panel lateral: carpetas y playlists
        auto side = b.removeFromLeft (juce::jmax (200.0f, b.getWidth() * 0.2f));
        g.setColour (panel);
        g.fillRoundedRectangle (side, 4.0f);
        side = side.reduced (rowH * 0.4f, rowH * 0.2f);
        draw::text (g, tr ("library.sidebar"), side.removeFromTop (rowH), textH * 0.85f, textSecondary, Justification::centredLeft);

        const juce::String items[] = { tr ("library.allTracks"), tr ("library.playlists"), tr ("library.history") };
        for (int i = 0; i < 3; ++i)
        {
            auto item = side.removeFromTop (rowH);
            if (i == 0)
            {
                g.setColour (cyan.withAlpha (0.15f));
                g.fillRoundedRectangle (item, 3.0f);
            }
            auto icon = item.removeFromLeft (rowH).reduced (rowH * 0.3f);
            g.setColour (i == 0 ? cyan : textSecondary);
            g.fillRoundedRectangle (icon, 2.0f);
            draw::text (g, items[i], item, textH, i == 0 ? textPrimary : textSecondary, Justification::centredLeft, false);
        }

        b.removeFromLeft (4.0f);

        // Lista de pistas: encabezado de columnas
        g.setColour (panel);
        g.fillRoundedRectangle (b, 4.0f);
        auto header = b.removeFromTop (rowH);
        g.setColour (panelLight);
        g.fillRect (header);

        struct Col { const char* key; float weight; };
        static constexpr Col cols[] = { { "library.col.number", 0.4f }, { "library.col.artwork", 0.8f },
                                        { "library.col.title", 3.0f }, { "library.col.artist", 2.4f },
                                        { "library.col.bpm", 0.7f }, { "library.col.key", 0.6f },
                                        { "library.col.duration", 0.9f }, { "library.col.genre", 1.4f },
                                        { "library.col.rating", 0.9f } };
        float total = 0.0f;
        for (const auto& c : cols) total += c.weight;

        auto x = header.getX();
        for (const auto& c : cols)
        {
            const auto w = header.getWidth() * c.weight / total;
            draw::text (g, tr (c.key), { x + 6.0f, header.getY(), w - 8.0f, header.getHeight() },
                        textH * 0.85f, textSecondary, Justification::centredLeft);
            g.setColour (panelEdge);
            g.drawVerticalLine ((int) (x + w), header.getY() + 3.0f, header.getBottom() - 3.0f);
            x += w;
        }

        // Filas vacías alternadas + indicación de arrastrar y soltar
        for (float y = b.getY(); y + rowH <= b.getBottom(); y += rowH * 2.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.02f));
            g.fillRect (b.getX(), y + rowH, b.getWidth(), rowH);
        }
        draw::text (g, tr ("library.dropHint"), b, textH, textSecondary.withAlpha (0.8f), Justification::centred, false);
    }

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
