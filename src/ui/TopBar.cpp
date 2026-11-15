#include "ui/TopBar.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/Palette.h"

namespace kb::ui
{
    using namespace palette;

    TopBar::TopBar()
    {
        setOpaque (true);
        meterL.setTooltipKey ("tip.masterMeter");
        meterR.setTooltipKey ("tip.masterMeter");
        addAndMakeVisible (meterL);
        addAndMakeVisible (meterR);
    }

    juce::String TopBar::getTooltip()
    {
        return limitArea.expanded (4.0f).contains (getMouseXYRelative().toFloat()) ? tr ("tip.limit") : juce::String();
    }

    void TopBar::pushMaster (const engine::MeterSource::Reading& l, const engine::MeterSource::Reading& r,
                             float reductionDb, float dt)
    {
        meterL.push (l, dt);
        meterR.push (r, dt);

        // LED "LIMIT": se enciende cuando el limitador reduce más de 0,5 dB y queda 300 ms.
        if (reductionDb > 0.5f)
            limitHold = 0.3f;
        else
            limitHold = juce::jmax (0.0f, limitHold - dt);

        const auto now = limitHold > 0.0f;
        if (now != limiting)
        {
            limiting = now;
            repaint (limitArea.toNearestInt().expanded (6));
        }
    }

    void TopBar::resized()
    {
        const auto b = getLocalBounds().toFloat();
        const auto h = b.getHeight();
        auto area = b.reduced (h * 0.4f, h * 0.12f);
        const auto sideW = area.getWidth() * 0.3f;
        area.removeFromLeft (sideW);
        auto right = area.removeFromRight (sideW);
        limitArea = right.removeFromLeft (h * 1.6f).withSizeKeepingCentre (h * 1.4f, h * 0.42f);

        meterArea = area.reduced (area.getWidth() * 0.06f, 0.0f);
        auto m = meterArea;
        scaleArea = m.removeFromBottom (m.getHeight() * 0.3f);
        const auto rowH = m.getHeight() * 0.44f;
        meterL.setBounds (m.removeFromTop (rowH).toNearestInt());
        meterR.setBounds (m.removeFromBottom (rowH).toNearestInt());
    }

    void TopBar::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat();
        const auto h = b.getHeight();

        g.setGradientFill (juce::ColourGradient (panelLight, 0.0f, 0.0f, panelDark, 0.0f, h, false));
        g.fillRect (b);
        g.setColour (chassis.withAlpha (0.55f));
        g.fillRect (b.withTop (b.getBottom() - juce::jmax (1.0f, h * 0.04f)));

        auto area = b.reduced (h * 0.4f, h * 0.12f);
        const auto sideW = area.getWidth() * 0.3f;
        draw::text (g, tr ("app.logo"), area.removeFromLeft (sideW), h * 0.52f, chassis, juce::Justification::centredLeft);
        draw::text (g, tr ("app.brand"), area.removeFromRight (sideW), h * 0.34f, textSecondary, juce::Justification::centredRight);

        // Escala en dBFS debajo de los vúmetros (alineada con su barra de segmentos).
        const auto pad = juce::jmax (1.0f, meterL.getHeight() * 0.12f);
        const auto clipSize = (meterL.getHeight() - 2.0f * pad) * 1.6f;
        const auto x0 = meterArea.getX() + pad;
        const auto x1 = meterArea.getRight() - pad - clipSize - pad;
        g.setFont (juce::FontOptions (scaleArea.getHeight() * 0.95f));
        for (auto db : LevelMeter::scaleMarks())
        {
            const auto x = x0 + (x1 - x0) * LevelMeter::dbToPosition (db);
            g.setColour (textSecondary);
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (40.0f, scaleArea.getHeight()).withCentre ({ x, scaleArea.getCentreY() }),
                        juce::Justification::centred);
        }

        // LED LIMIT
        if (limiting)
        {
            g.setColour (alert.withAlpha (0.3f));
            g.fillRoundedRectangle (limitArea.expanded (3.0f), 4.0f);
        }
        g.setColour (limiting ? alert : alert.withAlpha (0.18f));
        g.fillRoundedRectangle (limitArea, 3.0f);
        draw::text (g, tr ("meter.limit"), limitArea, limitArea.getHeight() * 0.62f, limiting ? panelDark : textSecondary);
    }
} // namespace kb::ui
