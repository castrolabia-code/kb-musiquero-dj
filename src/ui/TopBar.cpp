#include "ui/TopBar.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/KbLookAndFeel.h"
#include "ui/Palette.h"

#include <tuple>

namespace kb::ui
{
    using namespace palette;

    TopBar::TopBar()
    {
        setOpaque (false);
        meterL.setTooltipKey ("tip.masterMeter");
        meterR.setTooltipKey ("tip.masterMeter");
        addAndMakeVisible (meterL);
        addAndMakeVisible (meterR);
        libraryButton.setTooltipKey ("tip.largeLibrary");
        libraryButton.setComponentID ("library.largeView");
        libraryButton.onPress = [this] { if (onLibraryView) onLibraryView(); };
        addAndMakeVisible (libraryButton);
    }

    // =========================================================================
    //  Logo: letras negras sobre el chasis amarillo, con un filo de luz abajo
    //  (como impresas en relieve), igual que la imagen de referencia.
    // =========================================================================
    static juce::Path fittedText (const juce::String& text, juce::Rectangle<float> area, float height, bool black,
                                  juce::Justification just)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (KbLookAndFeel::displayFont (height, black), text, 0.0f, 0.0f);
        juce::Path p;
        ga.createPath (p);
        const auto pb = p.getBounds();
        const auto fit = juce::jmin (area.getHeight() / pb.getHeight(), area.getWidth() / pb.getWidth());
        const auto w = pb.getWidth() * fit;
        const auto x = just.testFlags (juce::Justification::right) ? area.getRight() - w
                     : just.testFlags (juce::Justification::left)  ? area.getX()
                                                                   : area.getCentreX() - w * 0.5f;
        p.applyTransform (juce::AffineTransform::translation (-pb.getX(), -pb.getY()).scaled (fit)
                              .translated (x, area.getCentreY() - pb.getHeight() * fit * 0.5f));
        return p;
    }

    void TopBar::renderLogo (float scale)
    {
        logoScale = scale;
        logo = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                            juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
        juce::Graphics g (logo);
        g.addTransform (juce::AffineTransform::scale (scale));

        const auto h = logoArea.getHeight();
        for (auto [text, area, black, just] : { std::tuple (tr ("app.logo"), logoArea.withHeight (h * 0.5f).withY (logoArea.getY() + h * 0.2f), true, juce::Justification (juce::Justification::left)),
                                                std::tuple (tr ("app.brand"), brandArea.withHeight (h * 0.38f).withY (brandArea.getY() + h * 0.3f), true, juce::Justification (juce::Justification::right)) })
        {
            const auto p = fittedText (text, area, h, black, just);
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.fillPath (p, juce::AffineTransform::translation (0.0f, juce::jmax (1.0f, h * 0.025f)));
            g.setColour (juce::Colour (0xff0d0d0d));
            g.fillPath (p);

            // Línea fina debajo (como la serigrafía de la referencia).
            const auto pb = p.getBounds();
            const auto y = logoArea.getY() + h * 0.86f;
            g.setColour (chassisShadow.darker (0.35f).withAlpha (0.7f));
            g.fillRect (juce::Rectangle<float> (pb.getX(), y, pb.getWidth() * 1.12f, juce::jmax (1.0f, h * 0.02f))
                            .withX (just.testFlags (juce::Justification::right) ? pb.getRight() - pb.getWidth() * 1.12f : pb.getX()));
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.fillRect (juce::Rectangle<float> (pb.getX(), y + juce::jmax (1.0f, h * 0.02f), pb.getWidth() * 1.12f, 1.0f)
                            .withX (just.testFlags (juce::Justification::right) ? pb.getRight() - pb.getWidth() * 1.12f : pb.getX()));
        }
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
        auto area = b.reduced (h * 0.3f, h * 0.04f);
        logoArea  = area.removeFromLeft (area.getWidth() * 0.32f);
        brandArea = area.removeFromRight (area.getWidth() * 0.42f);
        // Botón "BIBLIOTECA" entre el vúmetro y la marca.
        libraryButton.setBounds (brandArea.removeFromLeft (h * 2.6f).withSizeKeepingCentre (h * 2.3f, h * 0.48f).toNearestInt());
        libraryButton.setTextScale (0.5f);
        brandArea.removeFromLeft (h * 0.3f);
        logo = {};

        // Caja oscura del vúmetro (centro), con el LED LIMIT adentro a la derecha.
        boxArea = area.withSizeKeepingCentre (area.getWidth() - h * 1.2f, h * 0.86f);
        auto inner = boxArea.reduced (h * 0.14f, h * 0.08f);
        limitArea = inner.removeFromRight (h * 1.1f).withSizeKeepingCentre (h * 1.0f, h * 0.34f);
        inner.removeFromRight (h * 0.12f);
        meterArea = inner;
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

        juce::ignoreUnused (b);
        // Caja del vúmetro: negra, hundida en el chasis.
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRoundedRectangle (boxArea.translated (0.0f, 1.5f), h * 0.1f);
        g.setColour (juce::Colour (0xff050607));
        g.fillRoundedRectangle (boxArea, h * 0.1f);
        g.setColour (juce::Colour (0xff3a3d44));
        g.drawRoundedRectangle (boxArea.reduced (1.0f), h * 0.1f, 1.0f);

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
