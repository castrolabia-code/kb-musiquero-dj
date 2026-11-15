#include "ui/TopBar.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/KbLookAndFeel.h"
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

    // =========================================================================
    //  Logo neón 3D: extrusión, resplandor de tubo de neón y relleno con brillo.
    // =========================================================================
    void TopBar::renderLogo (float scale)
    {
        logoScale = scale;
        logo = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                            juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
        juce::Graphics g (logo);
        g.addTransform (juce::AffineTransform::scale (scale));

        // Texto -> contornos vectoriales en Orbitron Black, en 3 colores: KB / MUSIQUERO / DJ.
        const auto words = juce::StringArray::fromTokens (tr ("app.logo"), " ", "");
        const juce::Colour neon[] = { orange, cyan, chassis };
        const auto textH = logoArea.getHeight() * 0.62f;
        const auto font = KbLookAndFeel::displayFont (textH);

        juce::Path parts[3];
        float x = 0.0f;
        for (int i = 0; i < words.size() && i < 3; ++i)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (font, words[i], x, 0.0f);
            ga.createPath (parts[i]);
            x += ga.getBoundingBox (0, -1, true).getWidth() + textH * 0.35f;
        }
        juce::Path all;
        for (auto& p : parts) all.addPath (p);
        const auto bounds = all.getBounds();
        const auto fit = juce::jmin (1.0f, (logoArea.getWidth() * 0.96f) / bounds.getWidth());
        const auto t = juce::AffineTransform::translation (-bounds.getX(), -bounds.getY())
                           .scaled (fit)
                           .translated (logoArea.getX(), logoArea.getCentreY() - bounds.getHeight() * fit * 0.5f);
        for (auto& p : parts) p.applyTransform (t);

        const auto depth = juce::jmax (2, juce::roundToInt (textH * 0.12f));
        for (int i = 0; i < words.size() && i < 3; ++i)
        {
            const auto& p = parts[i];
            const auto c = neon[i];

            // Extrusión 3D hacia abajo-derecha.
            for (int k = depth; k >= 1; --k)
            {
                g.setColour (c.darker (2.2f).withAlpha (0.95f));
                g.fillPath (p, juce::AffineTransform::translation ((float) k * 0.55f, (float) k * 0.8f));
            }
            // Resplandor de neón (dos capas).
            juce::DropShadow (c.withAlpha (0.85f), juce::roundToInt (textH * 0.45f), {}).drawForPath (g, p);
            juce::DropShadow (c.withAlpha (0.9f), juce::roundToInt (textH * 0.15f), {}).drawForPath (g, p);
            // Relleno: centro casi blanco (tubo encendido) hacia el color.
            const auto pb = p.getBounds();
            juce::ColourGradient fill (c.interpolatedWith (juce::Colours::white, 0.7f), 0.0f, pb.getY(),
                                       c, 0.0f, pb.getBottom(), false);
            g.setGradientFill (fill);
            g.fillPath (p);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.strokePath (p, juce::PathStrokeType (juce::jmax (0.8f, textH * 0.025f)));
        }

        // "BY CASTRO LABS" con un brillo suave.
        juce::GlyphArrangement brand;
        brand.addFittedText (KbLookAndFeel::displayFont (brandArea.getHeight() * 0.32f, false), tr ("app.brand"),
                             brandArea.getX(), brandArea.getY(), brandArea.getWidth(), brandArea.getHeight(),
                             juce::Justification::centredRight, 1);
        juce::Path brandPath;
        brand.createPath (brandPath);
        juce::DropShadow (cyan.withAlpha (0.5f), juce::roundToInt (brandArea.getHeight() * 0.2f), {}).drawForPath (g, brandPath);
        g.setColour (textPrimary);
        g.fillPath (brandPath);
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
        auto area = b.reduced (h * 0.25f, h * 0.06f);
        const auto sideW = area.getWidth() * 0.34f;
        logoArea = area.removeFromLeft (sideW);
        auto right = area.removeFromRight (sideW * 0.82f);
        brandArea = right.withTrimmedLeft (h * 1.8f);
        logo = {};
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

        const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (logo.isNull() || ! juce::exactlyEqual (scale, logoScale))
            renderLogo (scale);
        g.setOpacity (1.0f);   // drawImage usa la opacidad del último color: el logo va al 100 %
        g.drawImage (logo, getLocalBounds().toFloat());

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
