#include "ui/ControllerView.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/Palette.h"

namespace kb::ui
{
    using juce::Colour;
    using juce::Graphics;
    using juce::Justification;
    using juce::Point;
    using juce::Rectangle;
    using namespace palette;

    ControllerView::ControllerView()
    {
        setOpaque (true);

        for (int d = 0; d < 2; ++d)
        {
            playButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.play", play, true);
            cueButtons[(size_t) d]  = std::make_unique<GlowButton> ("deck.cue", alert, true);
            volumeFaders[(size_t) d] = std::make_unique<KbFader> (true);

            playButtons[(size_t) d]->onPress   = [this, d] { if (onPlay) onPlay (d); };
            cueButtons[(size_t) d]->onPress    = [this, d] { if (onCueDown) onCueDown (d); };
            cueButtons[(size_t) d]->onRelease  = [this, d] { if (onCueUp) onCueUp (d); };

            auto& fader = *volumeFaders[(size_t) d];
            fader.setDoubleClickReturnValue (true, 1.0);
            fader.onValueChange = [this, d, &fader] { if (onVolume) onVolume (d, (float) fader.getValue()); };

            addAndMakeVisible (*playButtons[(size_t) d]);
            addAndMakeVisible (*cueButtons[(size_t) d]);
            addAndMakeVisible (fader);
        }

        crossfader.setRange (-1.0, 1.0);
        crossfader.setDoubleClickReturnValue (true, 0.0);
        crossfader.onValueChange = [this] { if (onCrossfader) onCrossfader ((float) crossfader.getValue()); };
        addAndMakeVisible (crossfader);

        masterKnob.setDoubleClickReturnValue (true, 0.85);
        masterKnob.onValueChange = [this] { if (onMaster) onMaster ((float) masterKnob.getValue()); };
        addAndMakeVisible (masterKnob);
    }

    // =========================================================================
    //  Estado
    // =========================================================================
    void ControllerView::setDeckLights (int deck, const DeckLights& l, bool blinkPhase)
    {
        if (! juce::isPositiveAndBelow (deck, 2))
            return;

        // PLAY: encendido reproduciendo; titila en pausa con pista cargada (CDJ).
        playButtons[(size_t) deck]->setLit (l.playing || (l.loaded && ! l.playing && blinkPhase));
        // CUE: encendido en pausa sobre el cue o durante la preescucha.
        cueButtons[(size_t) deck]->setLit (l.loaded && (l.cuePreview || (! l.playing && l.atCue)));
    }

    void ControllerView::setVolume (int deck, float v)
    {
        if (juce::isPositiveAndBelow (deck, 2))
            volumeFaders[(size_t) deck]->setValue (v, juce::dontSendNotification);
    }

    void ControllerView::setCrossfader (float v) { crossfader.setValue (v, juce::dontSendNotification); }
    void ControllerView::setMaster (float v)     { masterKnob.setValue (v, juce::dontSendNotification); }

    void ControllerView::setDropTarget (int deck)
    {
        if (dropTarget != deck)
        {
            dropTarget = deck;
            repaint();
        }
    }

    int ControllerView::deckAt (Point<int> p) const
    {
        for (int d = 0; d < 2; ++d)
            if (deckLayouts[(size_t) d].area.contains (p.toFloat()))
                return d;
        return -1;
    }

    // =========================================================================
    //  Layout (proporcional: unidad = alto / 100)
    // =========================================================================
    void ControllerView::resized()
    {
        computeLayout();

        for (int d = 0; d < 2; ++d)
        {
            const auto& dl = deckLayouts[(size_t) d];
            playButtons[(size_t) d]->setBounds (dl.play.expanded (dl.play.getHeight() * 0.08f).toNearestInt());
            cueButtons[(size_t) d]->setBounds (dl.cue.expanded (dl.cue.getHeight() * 0.08f).toNearestInt());
            volumeFaders[(size_t) d]->setBounds (mixer.faders[(size_t) d].toNearestInt());
        }

        crossfader.setBounds (mixer.crossfader.toNearestInt());
        masterKnob.setGeometry (mixer.masterRadius, unit * 2.6f * 0.8f);
        masterKnob.setBounds (masterKnob.boundsForCentre (mixer.masterCentre).toNearestInt());
    }

    void ControllerView::computeLayout()
    {
        auto b = getLocalBounds().toFloat();
        unit = b.getHeight() / 100.0f;
        chassis = b.reduced (unit * 1.5f);

        auto inner = chassis.reduced (unit * 3.0f);
        const auto gap   = unit * 2.0f;
        const auto deckW = inner.getWidth() * 0.375f;

        const auto deck1 = inner.removeFromLeft (deckW);
        inner.removeFromLeft (gap);
        const auto deck2 = inner.removeFromRight (deckW);
        inner.removeFromRight (gap);

        // --- Decks ---------------------------------------------------------------------
        for (int d = 0; d < 2; ++d)
        {
            auto& L = deckLayouts[(size_t) d];
            L.mirrored = d == 1;
            L.area = d == 0 ? deck1 : deck2;
            auto r = L.area;
            const auto textH = unit * 3.2f;

            auto tempoCol = L.mirrored ? r.removeFromRight (r.getWidth() * 0.11f) : r.removeFromLeft (r.getWidth() * 0.11f);
            if (L.mirrored) r.removeFromRight (unit * 1.5f); else r.removeFromLeft (unit * 1.5f);
            L.tempoLabel = tempoCol.removeFromTop (textH * 1.4f);
            L.tempoSlot  = tempoCol.reduced (0.0f, unit * 1.5f);

            L.header = r.removeFromTop (textH * 1.4f);

            auto platterArea = r.removeFromTop (r.getHeight() * 0.66f);
            const auto diameter = juce::jmin (platterArea.getWidth(), platterArea.getHeight()) * 0.96f;
            L.platter = Rectangle<float> (diameter, diameter).withCentre (platterArea.getCentre());

            r.removeFromTop (unit * 2.0f);
            auto transport = L.mirrored ? r.removeFromRight (r.getWidth() * 0.36f) : r.removeFromLeft (r.getWidth() * 0.36f);
            if (L.mirrored) r.removeFromRight (unit * 2.0f); else r.removeFromLeft (unit * 2.0f);

            const auto btnD  = juce::jmin (transport.getWidth() / 3.3f, transport.getHeight() * 0.85f);
            const auto cellW = transport.getWidth() / 3.0f;
            Rectangle<float>* cells[] = { &L.sync, &L.cue, &L.play };
            for (int i = 0; i < 3; ++i)
            {
                const auto cell = transport.withWidth (cellW).withX (transport.getX() + cellW * (float) i);
                *cells[i] = Rectangle<float> (btnD, btnD).withCentre (cell.getCentre());
            }

            auto modes = r.removeFromTop (r.getHeight() * 0.3f);
            const auto modeW = modes.getWidth() / 4.0f;
            for (int i = 0; i < 4; ++i)
                L.modes[(size_t) i] = modes.withWidth (modeW).withX (modes.getX() + modeW * (float) i).reduced (unit * 0.5f, unit * 0.6f);

            r.removeFromTop (unit * 1.0f);
            const auto padW = r.getWidth() / 4.0f;
            for (int i = 0; i < 4; ++i)
            {
                auto cell = r.withWidth (padW).withX (r.getX() + padW * (float) i).reduced (unit * 0.6f);
                const auto side = juce::jmin (cell.getWidth(), cell.getHeight());
                L.pads[(size_t) i] = cell.withSizeKeepingCentre (cell.getWidth(), side);
            }
        }

        // --- Mixer -----------------------------------------------------------------------
        auto& M = mixer;
        M.panel = inner;
        auto area = inner.reduced (unit * 1.5f);
        M.title = area.removeFromTop (unit * 5.0f);

        auto xfaderArea = area.removeFromBottom (unit * 10.0f);
        M.crossfader = xfaderArea.reduced (xfaderArea.getWidth() * 0.16f, unit * 0.8f);

        const auto stripW = area.getWidth() * 0.35f;
        Rectangle<float> strips[2] = { area.removeFromLeft (stripW), area.removeFromRight (stripW) };
        auto centre = area;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto s = strips[ch];
            auto knobs = s.removeFromTop (s.getHeight() * 0.64f);
            const auto rowH = knobs.getHeight() / 5.0f;
            M.eqRadius = juce::jmin (rowH * 0.21f, s.getWidth() * 0.2f);
            for (int i = 0; i < 5; ++i)
                M.eqCentres[(size_t) ch][(size_t) i] = { knobs.getCentreX(), knobs.getY() + rowH * ((float) i + 0.36f) };

            auto cueArea = s.removeFromTop (s.getHeight() * 0.2f);
            M.cueButtons[(size_t) ch] = cueArea.withSizeKeepingCentre (s.getWidth() * 0.6f, juce::jmin (cueArea.getHeight() * 0.7f, unit * 5.0f));
            M.faders[(size_t) ch] = s.reduced (s.getWidth() * 0.3f, 0.0f).withTrimmedTop (unit * 0.8f);
        }

        auto masterArea = centre.removeFromTop (centre.getHeight() * 0.2f);
        M.masterRadius = juce::jmin (masterArea.getHeight() * 0.26f, centre.getWidth() * 0.18f);
        M.masterCentre = { masterArea.getCentreX(), masterArea.getCentreY() - unit };

        auto phonesArea = centre.removeFromBottom (centre.getHeight() * 0.28f);
        M.smallRadius  = juce::jmin (phonesArea.getHeight() * 0.2f, centre.getWidth() * 0.13f);
        M.phonesCentre = { phonesArea.getX() + phonesArea.getWidth() * 0.27f, phonesArea.getCentreY() - unit };
        M.cueMixCentre = { phonesArea.getX() + phonesArea.getWidth() * 0.73f, phonesArea.getCentreY() - unit };

        auto meters = centre.reduced (centre.getWidth() * 0.28f, unit * 1.0f);
        const auto mW = meters.getWidth() * 0.4f;
        M.meterL = meters.withWidth (mW);
        M.meterR = meters.withTrimmedLeft (meters.getWidth() - mW);
    }

    // =========================================================================
    //  Pintado de las partes estáticas
    // =========================================================================
    void ControllerView::paint (Graphics& g)
    {
        g.fillAll (panelDark);

        // Chasis amarillo con sombreado 3D (será un render PNG en la Fase 7)
        juce::Path chassisPath;
        chassisPath.addRoundedRectangle (chassis, unit * 3.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), juce::roundToInt (unit * 2.5f), { 0, juce::roundToInt (unit) })
            .drawForPath (g, chassisPath);

        juce::ColourGradient grad (chassisLight, 0.0f, chassis.getY(), chassisShadow, 0.0f, chassis.getBottom(), false);
        grad.addColour (0.35, palette::chassis);
        g.setGradientFill (grad);
        g.fillPath (chassisPath);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (chassis.reduced (unit * 0.4f), unit * 2.8f, juce::jmax (1.0f, unit * 0.25f));
        g.setColour (chassisShadow.darker (0.4f));
        g.drawRoundedRectangle (chassis, unit * 3.0f, juce::jmax (1.0f, unit * 0.2f));

        for (int d = 0; d < 2; ++d)
            paintDeck (g, deckLayouts[(size_t) d], d);
        paintMixer (g);
    }

    void ControllerView::paintDeck (Graphics& g, const DeckLayout& L, int deckIndex) const
    {
        const auto textH  = unit * 3.2f;
        const auto accent = deckColour (deckIndex);

        // TEMPO (Fase 3)
        draw::text (g, tr ("deck.tempo"), L.tempoLabel, textH * 0.85f, textOnChassis);
        g.setColour (panelDark.withAlpha (0.85f));
        g.fillRoundedRectangle (L.tempoSlot, unit * 1.2f);
        draw::fader (g, L.tempoSlot.reduced (unit * 0.8f, unit * 2.0f), true, 0.5f);

        draw::text (g, tr ("deck.name", { { "n", juce::String (deckIndex + 1) } }), L.header, textH,
                    textOnChassis, L.mirrored ? Justification::centredRight : Justification::centredLeft);

        // Plato / jog wheel (animado en la Fase 4)
        const auto& platter = L.platter;
        const auto diameter = platter.getWidth();
        const auto c = platter.getCentre();

        juce::Path disc;
        disc.addEllipse (platter);
        juce::DropShadow (juce::Colours::black.withAlpha (0.65f), juce::roundToInt (unit * 3.0f), { 0, juce::roundToInt (unit) })
            .drawForPath (g, disc);
        g.setGradientFill (juce::ColourGradient (panelEdge.brighter (0.6f), platter.getX(), platter.getY(),
                                                 panelDark, platter.getRight(), platter.getBottom(), false));
        g.fillPath (disc);

        const auto ringR = diameter * 0.455f;
        juce::Path ring;
        ring.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
        juce::DropShadow (orange.withAlpha (0.7f), juce::roundToInt (unit * 1.6f), {}).drawForPath (g, ring);
        g.setColour (orange);
        g.strokePath (ring, juce::PathStrokeType (juce::jmax (1.5f, unit * 0.9f)));

        const auto vinyl = platter.reduced (diameter * 0.075f);
        g.setGradientFill (juce::ColourGradient (panel, c.x, c.y, slot, vinyl.getRight(), c.y, true));
        g.fillEllipse (vinyl);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        for (float k = 0.36f; k < 0.98f; k += 0.055f)
            g.drawEllipse (vinyl.withSizeKeepingCentre (vinyl.getWidth() * k, vinyl.getHeight() * k), 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), vinyl.getX(), vinyl.getY(),
                                                 juce::Colours::transparentWhite, c.x, c.y, false));
        g.fillEllipse (vinyl);

        const auto label = vinyl.withSizeKeepingCentre (vinyl.getWidth() * 0.3f, vinyl.getHeight() * 0.3f);
        g.setColour (panelDark);
        g.fillEllipse (label);
        g.setColour (accent);
        g.drawEllipse (label, juce::jmax (1.0f, unit * 0.4f));
        draw::text (g, juce::String (deckIndex + 1), label, label.getHeight() * 0.5f, accent);
        g.setColour (textPrimary);
        g.drawLine (c.x, vinyl.getY() + unit, c.x, vinyl.getY() + vinyl.getHeight() * 0.18f, juce::jmax (1.5f, unit * 0.5f));

        // SYNC (Fase 3). PLAY y CUE son componentes.
        draw::button (g, L.sync, cyan, tr ("deck.sync"), false, true, L.sync.getHeight() * 0.2f);

        // Modos de pads y pads (Fase 4)
        const char* modeKeys[] = { "deck.mode.hotcue", "deck.mode.fx", "deck.mode.sampler", "deck.mode.loop" };
        for (int i = 0; i < 4; ++i)
            draw::button (g, L.modes[(size_t) i], orange, tr (modeKeys[i]), i == 0, false, textH * 0.62f);
        for (int i = 0; i < 4; ++i)
            draw::button (g, L.pads[(size_t) i], cyan, juce::String (i + 1), false, false, L.pads[(size_t) i].getHeight() * 0.32f);

        // Destino de arrastrar y soltar
        if (dropTarget == deckIndex)
        {
            g.setColour (cyan.withAlpha (0.18f));
            g.fillRoundedRectangle (L.area.expanded (unit), unit * 2.0f);
            g.setColour (cyan);
            g.drawRoundedRectangle (L.area.expanded (unit), unit * 2.0f, juce::jmax (2.0f, unit * 0.6f));
            const auto msg = L.platter.withSizeKeepingCentre (L.platter.getWidth() * 1.2f, unit * 7.0f);
            g.setColour (panelDark.withAlpha (0.85f));
            g.fillRoundedRectangle (msg, unit);
            draw::text (g, tr ("wave.dropHere", { { "n", juce::String (deckIndex + 1) } }), msg.reduced (unit), unit * 3.2f, cyan);
        }
    }

    void ControllerView::paintMixer (Graphics& g) const
    {
        const auto& M = mixer;

        juce::Path panelPath;
        panelPath.addRoundedRectangle (M.panel, unit * 2.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), juce::roundToInt (unit * 1.5f), {}).drawForPath (g, panelPath);
        g.setGradientFill (juce::ColourGradient (panelLight, 0.0f, M.panel.getY(), panelDark, 0.0f, M.panel.getBottom(), false));
        g.fillPath (panelPath);
        g.setColour (panelEdge);
        g.strokePath (panelPath, juce::PathStrokeType (juce::jmax (1.0f, unit * 0.25f)));

        const auto textH = unit * 2.6f;
        draw::text (g, tr ("mixer.title"), M.title, unit * 3.6f, textPrimary);

        // EQ / gain / filtro (Fase 2)
        const char* eqKeys[] = { "mixer.gain", "mixer.high", "mixer.mid", "mixer.low", "mixer.filter" };
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 5; ++i)
                draw::knob (g, M.eqCentres[(size_t) ch][(size_t) i], M.eqRadius, 0.5f, tr (eqKeys[i]), textH * 0.8f);
            draw::button (g, M.cueButtons[(size_t) ch], cyan, tr ("mixer.cue"), false, false, textH * 0.8f);
        }

        // Auriculares (Fase 2) y vúmetros (Fase 2)
        draw::knob (g, M.phonesCentre, M.smallRadius, 0.5f, tr ("mixer.headphones"), textH * 0.7f);
        draw::knob (g, M.cueMixCentre, M.smallRadius, 0.5f, tr ("mixer.cueMix"), textH * 0.7f);
        draw::meter (g, M.meterL, true, 16);
        draw::meter (g, M.meterR, true, 16);
    }
} // namespace kb::ui
