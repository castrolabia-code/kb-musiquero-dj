#include "ui/SkinPlaceholders.h"

#include "app/I18n.h"
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
    //  Primitivas de dibujo reutilizables (vectoriales)
    // =========================================================================
    namespace draw
    {
        static constexpr float kArcStart = -juce::MathConstants<float>::pi * 0.75f;
        static constexpr float kArcEnd   =  juce::MathConstants<float>::pi * 0.75f;

        static void text (Graphics& g, const juce::String& s, Rectangle<float> r, float height,
                          Colour c, Justification j = Justification::centred, bool bold = true)
        {
            g.setColour (c);
            g.setFont (KbLookAndFeel::labelFont (height, bold));
            g.drawFittedText (s, r.toNearestInt(), j, 1, 0.8f);
        }

        /** Perilla con arco de valor naranja (sección 6). value01 en [0, 1]. */
        static void knob (Graphics& g, Point<float> c, float r, float value01, const juce::String& label, float textH)
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
        static void button (Graphics& g, Rectangle<float> r, Colour colour, const juce::String& label,
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
        static void fader (Graphics& g, Rectangle<float> r, bool vertical, float pos01)
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
        static Colour meterColour (float t)
        {
            if (t < 0.60f) return meterGreen;
            if (t < 0.80f) return meterYellow;
            if (t < 0.92f) return meterOrange;
            return meterRed;
        }

        /** Columna/fila de LEDs apagados (los vúmetros reales llegan en la Fase 2). */
        static void meter (Graphics& g, Rectangle<float> r, bool vertical, int segments)
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

    // =========================================================================
    //  Barra superior
    // =========================================================================
    TopBarPlaceholder::TopBarPlaceholder() { setOpaque (true); }

    void TopBarPlaceholder::paint (Graphics& g)
    {
        const auto b = getLocalBounds().toFloat();
        const auto h = b.getHeight();

        g.setGradientFill (juce::ColourGradient (panelLight, 0.0f, 0.0f, panelDark, 0.0f, h, false));
        g.fillRect (b);
        g.setColour (chassis.withAlpha (0.55f));
        g.fillRect (b.withTop (b.getBottom() - juce::jmax (1.0f, h * 0.04f)));

        auto area = b.reduced (h * 0.4f, h * 0.12f);
        const auto sideW = area.getWidth() * 0.3f;

        draw::text (g, tr ("app.logo"), area.removeFromLeft (sideW), h * 0.52f, chassis, Justification::centredLeft);
        draw::text (g, tr ("app.brand"), area.removeFromRight (sideW), h * 0.34f, textSecondary, Justification::centredRight);

        // Vúmetro master (L / R)
        const auto meterArea = area.reduced (area.getWidth() * 0.08f, h * 0.1f);
        const auto rowH = meterArea.getHeight() * 0.42f;
        draw::meter (g, meterArea.withHeight (rowH), false, 40);
        draw::meter (g, meterArea.withTrimmedTop (meterArea.getHeight() - rowH), false, 40);
    }

    // =========================================================================
    //  Controlador
    // =========================================================================
    ControllerPlaceholder::ControllerPlaceholder() { setOpaque (true); }

    void ControllerPlaceholder::paint (Graphics& g)
    {
        g.fillAll (panelDark);

        auto b = getLocalBounds().toFloat();
        const auto unit = b.getHeight() / 100.0f;   // unidad de layout proporcional
        b = b.reduced (unit * 1.5f);

        // Chasis amarillo con sombreado 3D (será un render PNG en la Fase 7)
        juce::Path chassisPath;
        chassisPath.addRoundedRectangle (b, unit * 3.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), juce::roundToInt (unit * 2.5f), { 0, juce::roundToInt (unit) })
            .drawForPath (g, chassisPath);

        juce::ColourGradient grad (chassisLight, 0.0f, b.getY(), chassisShadow, 0.0f, b.getBottom(), false);
        grad.addColour (0.35, chassis);
        g.setGradientFill (grad);
        g.fillPath (chassisPath);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (unit * 0.4f), unit * 2.8f, juce::jmax (1.0f, unit * 0.25f));
        g.setColour (chassisShadow.darker (0.4f));
        g.drawRoundedRectangle (b, unit * 3.0f, juce::jmax (1.0f, unit * 0.2f));

        auto inner = b.reduced (unit * 3.0f);
        const auto gap   = unit * 2.0f;
        const auto deckW = inner.getWidth() * 0.375f;

        const auto deck1 = inner.removeFromLeft (deckW);
        inner.removeFromLeft (gap);
        const auto deck2 = inner.removeFromRight (deckW);
        inner.removeFromRight (gap);

        paintDeck (g, deck1, 0, false, unit);
        paintDeck (g, deck2, 1, true, unit);
        paintMixer (g, inner, unit);
    }

    void ControllerPlaceholder::paintDeck (Graphics& g, Rectangle<float> r, int deckIndex, bool mirrored, float unit) const
    {
        const auto textH = unit * 3.2f;
        const auto accent = deckColour (deckIndex);

        // Columna de TEMPO en el borde exterior
        auto tempoCol = mirrored ? r.removeFromRight (r.getWidth() * 0.11f) : r.removeFromLeft (r.getWidth() * 0.11f);
        if (mirrored) r.removeFromRight (unit * 1.5f); else r.removeFromLeft (unit * 1.5f);

        draw::text (g, tr ("deck.tempo"), tempoCol.removeFromTop (textH * 1.4f), textH * 0.85f, textOnChassis);
        auto tempoSlot = tempoCol.reduced (0.0f, unit * 1.5f);
        g.setColour (panelDark.withAlpha (0.85f));
        g.fillRoundedRectangle (tempoSlot, unit * 1.2f);
        draw::fader (g, tempoSlot.reduced (unit * 0.8f, unit * 2.0f), true, 0.5f);

        // Encabezado del deck
        auto header = r.removeFromTop (textH * 1.4f);
        draw::text (g, tr ("deck.name", { { "n", juce::String (deckIndex + 1) } }), header, textH,
                    textOnChassis, mirrored ? Justification::centredRight : Justification::centredLeft);

        // Plato / jog wheel
        auto platterArea = r.removeFromTop (r.getHeight() * 0.66f);
        const auto diameter = juce::jmin (platterArea.getWidth(), platterArea.getHeight()) * 0.96f;
        const auto platter = Rectangle<float> (diameter, diameter).withCentre (platterArea.getCentre());
        const auto c = platter.getCentre();

        juce::Path disc;
        disc.addEllipse (platter);
        juce::DropShadow (juce::Colours::black.withAlpha (0.65f), juce::roundToInt (unit * 3.0f), { 0, juce::roundToInt (unit) })
            .drawForPath (g, disc);

        // Base metálica
        g.setGradientFill (juce::ColourGradient (panelEdge.brighter (0.6f), platter.getX(), platter.getY(),
                                                 panelDark, platter.getRight(), platter.getBottom(), false));
        g.fillPath (disc);

        // Anillo luminoso naranja (será dinámico en la Fase 4)
        const auto ringR = diameter * 0.455f;
        juce::Path ring;
        ring.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
        juce::DropShadow (orange.withAlpha (0.7f), juce::roundToInt (unit * 1.6f), {}).drawForPath (g, ring);
        g.setColour (orange);
        g.strokePath (ring, juce::PathStrokeType (juce::jmax (1.5f, unit * 0.9f)));

        // Vinilo con surcos
        const auto vinyl = platter.reduced (diameter * 0.075f);
        g.setGradientFill (juce::ColourGradient (panel, c.x, c.y, slot, vinyl.getRight(), c.y, true));
        g.fillEllipse (vinyl);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        for (float k = 0.36f; k < 0.98f; k += 0.055f)
            g.drawEllipse (vinyl.withSizeKeepingCentre (vinyl.getWidth() * k, vinyl.getHeight() * k), 1.0f);

        // Reflejo sutil
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), vinyl.getX(), vinyl.getY(),
                                                 juce::Colours::transparentWhite, c.x, c.y, false));
        g.fillEllipse (vinyl);

        // Etiqueta central y marca de posición
        const auto label = vinyl.withSizeKeepingCentre (vinyl.getWidth() * 0.3f, vinyl.getHeight() * 0.3f);
        g.setColour (panelDark);
        g.fillEllipse (label);
        g.setColour (accent);
        g.drawEllipse (label, juce::jmax (1.0f, unit * 0.4f));
        draw::text (g, juce::String (deckIndex + 1), label, label.getHeight() * 0.5f, accent);
        g.setColour (textPrimary);
        g.drawLine (c.x, vinyl.getY() + unit, c.x, vinyl.getY() + vinyl.getHeight() * 0.18f, juce::jmax (1.5f, unit * 0.5f));

        // Zona inferior: transporte (lado exterior) + modos y pads (lado interior)
        r.removeFromTop (unit * 2.0f);
        auto transport = mirrored ? r.removeFromRight (r.getWidth() * 0.36f) : r.removeFromLeft (r.getWidth() * 0.36f);
        if (mirrored) r.removeFromRight (unit * 2.0f); else r.removeFromLeft (unit * 2.0f);

        // SYNC / CUE / PLAY
        const auto btnD = juce::jmin (transport.getWidth() / 3.3f, transport.getHeight() * 0.85f);
        const juce::String labels[] = { tr ("deck.sync"), tr ("deck.cue"), tr ("deck.play") };
        const Colour colours[] = { cyan, alert, play };
        for (int i = 0; i < 3; ++i)
        {
            const auto cell = transport.withWidth (transport.getWidth() / 3.0f)
                                       .withX (transport.getX() + transport.getWidth() / 3.0f * (float) i);
            draw::button (g, Rectangle<float> (btnD, btnD).withCentre (cell.getCentre()), colours[i], labels[i],
                          i == 2, true, btnD * 0.2f);
        }

        // Botones de modo de pads
        auto modes = r.removeFromTop (r.getHeight() * 0.3f);
        const juce::String modeLabels[] = { tr ("deck.mode.hotcue"), tr ("deck.mode.fx"),
                                            tr ("deck.mode.sampler"), tr ("deck.mode.loop") };
        const auto modeW = modes.getWidth() / 4.0f;
        for (int i = 0; i < 4; ++i)
            draw::button (g, modes.withWidth (modeW).withX (modes.getX() + modeW * (float) i).reduced (unit * 0.5f, unit * 0.6f),
                          orange, modeLabels[i], i == 0, false, textH * 0.62f);

        // 4 Performance Pads
        r.removeFromTop (unit * 1.0f);
        const auto padW = r.getWidth() / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            auto cell = r.withWidth (padW).withX (r.getX() + padW * (float) i).reduced (unit * 0.6f);
            const auto side = juce::jmin (cell.getWidth(), cell.getHeight());
            draw::button (g, cell.withSizeKeepingCentre (cell.getWidth(), side), cyan, juce::String (i + 1),
                          i == 0, false, side * 0.32f);
        }
    }

    void ControllerPlaceholder::paintMixer (Graphics& g, Rectangle<float> r, float unit) const
    {
        // Panel del mixer (carbón)
        juce::Path panelPath;
        panelPath.addRoundedRectangle (r, unit * 2.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), juce::roundToInt (unit * 1.5f), {}).drawForPath (g, panelPath);
        g.setGradientFill (juce::ColourGradient (panelLight, 0.0f, r.getY(), panelDark, 0.0f, r.getBottom(), false));
        g.fillPath (panelPath);
        g.setColour (panelEdge);
        g.strokePath (panelPath, juce::PathStrokeType (juce::jmax (1.0f, unit * 0.25f)));

        const auto textH = unit * 2.6f;
        auto area = r.reduced (unit * 1.5f);
        draw::text (g, tr ("mixer.title"), area.removeFromTop (unit * 5.0f), unit * 3.6f, textPrimary);

        // Crossfader abajo
        auto xfaderArea = area.removeFromBottom (unit * 10.0f);
        draw::fader (g, xfaderArea.reduced (xfaderArea.getWidth() * 0.18f, unit * 1.5f), false, 0.5f);

        const auto stripW = area.getWidth() * 0.35f;
        auto strip1 = area.removeFromLeft (stripW);
        auto strip2 = area.removeFromRight (stripW);
        auto centre = area;

        auto paintStrip = [&] (Rectangle<float> s, int ch)
        {
            const juce::String knobLabels[] = { tr ("mixer.gain"), tr ("mixer.high"), tr ("mixer.mid"),
                                                tr ("mixer.low"), tr ("mixer.filter") };
            auto knobs = s.removeFromTop (s.getHeight() * 0.64f);
            const auto rowH = knobs.getHeight() / 5.0f;
            const auto radius = juce::jmin (rowH * 0.21f, s.getWidth() * 0.2f);
            for (int i = 0; i < 5; ++i)
                draw::knob (g, { knobs.getCentreX(), knobs.getY() + rowH * ((float) i + 0.36f) }, radius,
                            0.5f, knobLabels[i], textH * 0.8f);

            auto cueBtn = s.removeFromTop (s.getHeight() * 0.2f);
            draw::button (g, cueBtn.withSizeKeepingCentre (s.getWidth() * 0.6f, juce::jmin (cueBtn.getHeight() * 0.7f, unit * 5.0f)),
                          cyan, tr ("mixer.cue"), ch == 0, false, textH * 0.8f);

            draw::fader (g, s.reduced (s.getWidth() * 0.25f, unit * 0.5f), true, 0.75f);
        };
        paintStrip (strip1, 0);
        paintStrip (strip2, 1);

        // Columna central: MASTER, vúmetros por canal, PHONES / CUE MIX
        auto masterArea = centre.removeFromTop (centre.getHeight() * 0.2f);
        draw::knob (g, { masterArea.getCentreX(), masterArea.getCentreY() - unit }, juce::jmin (masterArea.getHeight() * 0.26f, centre.getWidth() * 0.18f),
                    0.7f, tr ("mixer.master"), textH * 0.8f);

        auto phonesArea = centre.removeFromBottom (centre.getHeight() * 0.28f);
        const auto smallR = juce::jmin (phonesArea.getHeight() * 0.2f, centre.getWidth() * 0.13f);
        draw::knob (g, { phonesArea.getX() + phonesArea.getWidth() * 0.27f, phonesArea.getCentreY() - unit }, smallR,
                    0.5f, tr ("mixer.headphones"), textH * 0.7f);
        draw::knob (g, { phonesArea.getX() + phonesArea.getWidth() * 0.73f, phonesArea.getCentreY() - unit }, smallR,
                    0.5f, tr ("mixer.cueMix"), textH * 0.7f);

        auto meters = centre.reduced (centre.getWidth() * 0.28f, unit * 1.0f);
        const auto mW = meters.getWidth() * 0.4f;
        draw::meter (g, meters.withWidth (mW), true, 16);
        draw::meter (g, meters.withTrimmedLeft (meters.getWidth() - mW), true, 16);
    }

    // =========================================================================
    //  Waveforms
    // =========================================================================
    WaveformPlaceholder::WaveformPlaceholder() { setOpaque (true); }

    void WaveformPlaceholder::paint (Graphics& g)
    {
        g.fillAll (panelDark);

        auto b = getLocalBounds().toFloat().reduced (2.0f);
        const auto rowH = b.getHeight() / 2.0f;

        for (int deck = 0; deck < 2; ++deck)
        {
            auto row = b.removeFromTop (rowH).reduced (0.0f, 2.0f);
            const auto colour = deckColour (deck);
            const auto h = row.getHeight();

            // Info del deck
            auto info = row.removeFromLeft (juce::jmax (h * 4.2f, row.getWidth() * 0.22f));
            g.setColour (panel);
            g.fillRoundedRectangle (info, 4.0f);
            info = info.reduced (h * 0.1f);

            auto art = info.removeFromLeft (info.getHeight());
            g.setColour (panelEdge);
            g.fillRoundedRectangle (art, 3.0f);
            draw::text (g, juce::String (deck + 1), art, art.getHeight() * 0.5f, colour);
            info.removeFromLeft (h * 0.12f);

            const auto lineH = info.getHeight() / 3.0f;
            draw::text (g, tr ("wave.empty"), info.removeFromTop (lineH), lineH * 0.62f, textPrimary, Justification::centredLeft, false);
            draw::text (g, tr ("wave.bpm") + " --.-    " + tr ("wave.key") + " --    +0.00%",
                        info.removeFromTop (lineH), lineH * 0.55f, colour, Justification::centredLeft);
            draw::text (g, "00:00.0  /  -00:00.0", info, lineH * 0.55f, textSecondary, Justification::centredLeft);

            row.removeFromLeft (4.0f);

            // Waveform ampliada + vista general (vacías)
            auto overview = row.removeFromBottom (row.getHeight() * 0.22f);
            row.removeFromBottom (2.0f);

            g.setColour (slot);
            g.fillRoundedRectangle (row, 4.0f);
            g.fillRoundedRectangle (overview, 3.0f);

            g.setColour (colour.withAlpha (0.25f));
            g.drawHorizontalLine ((int) row.getCentreY(), row.getX(), row.getRight());
            g.drawHorizontalLine ((int) overview.getCentreY(), overview.getX(), overview.getRight());

            // Playhead central
            g.setColour (textPrimary.withAlpha (0.8f));
            g.fillRect (Rectangle<float> (2.0f, row.getHeight()).withCentre (row.getCentre()));
        }
    }

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
