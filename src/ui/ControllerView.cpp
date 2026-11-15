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
            playButtons[(size_t) d]  = std::make_unique<GlowButton> ("deck.play", play, true, ButtonIcon::play);
            pauseButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.pause", orange, true, ButtonIcon::pause);
            cueButtons[(size_t) d]   = std::make_unique<GlowButton> ("deck.cue", alert, true, ButtonIcon::cue);
            volumeFaders[(size_t) d] = std::make_unique<KbFader> (true);

            playButtons[(size_t) d]->setTooltipKey ("tip.play");
            pauseButtons[(size_t) d]->setTooltipKey ("tip.pause");
            cueButtons[(size_t) d]->setTooltipKey ("tip.cue");
            volumeFaders[(size_t) d]->setTooltipKey ("tip.volume");

            playButtons[(size_t) d]->onPress   = [this, d] { if (onPlay) onPlay (d); };
            pauseButtons[(size_t) d]->onPress  = [this, d] { if (onPause) onPause (d); };
            cueButtons[(size_t) d]->onPress    = [this, d] { if (onCueDown) onCueDown (d); };
            cueButtons[(size_t) d]->onRelease  = [this, d] { if (onCueUp) onCueUp (d); };

            auto& fader = *volumeFaders[(size_t) d];
            fader.setDoubleClickReturnValue (true, 1.0);
            fader.onValueChange = [this, d, &fader] { if (onVolume) onVolume (d, (float) fader.getValue()); };

            addAndMakeVisible (*playButtons[(size_t) d]);
            addAndMakeVisible (*pauseButtons[(size_t) d]);
            addAndMakeVisible (*cueButtons[(size_t) d]);
            addAndMakeVisible (fader);
        }

        crossfader.setTooltipKey ("tip.crossfader");
        masterKnob.setTooltipKey ("tip.master");
        crossfader.setRange (-1.0, 1.0);
        crossfader.setDoubleClickReturnValue (true, 0.0);
        crossfader.onValueChange = [this] { if (onCrossfader) onCrossfader ((float) crossfader.getValue()); };
        addAndMakeVisible (crossfader);

        masterKnob.setDoubleClickReturnValue (true, 0.85);
        masterKnob.onValueChange = [this] { if (onMaster) onMaster ((float) masterKnob.getValue()); };
        addAndMakeVisible (masterKnob);

        // --- Tempo, SYNC y platos (Fase 3) -------------------------------------------------
        for (int d = 0; d < 2; ++d)
        {
            platters[(size_t) d] = std::make_unique<PlatterView> (d);
            addAndMakeVisible (*platters[(size_t) d]);

            auto fader = std::make_unique<KbFader> (true);
            fader->setRange (-1.0, 1.0);
            fader->setDoubleClickReturnValue (true, 0.0);
            fader->setTooltipKey ("tip.tempo");
            auto* rawFader = fader.get();
            // Como en un CDJ: hacia abajo = más rápido (+), hacia arriba = más lento (-).
            fader->onValueChange = [this, d, rawFader]
            {
                tempoPercent[(size_t) d] = (float) -rawFader->getValue() * kTempoRanges[rangeIndex[(size_t) d]];
                if (onTempo) onTempo (d, tempoPercent[(size_t) d]);
            };
            addAndMakeVisible (*fader);
            tempoFaders[(size_t) d] = std::move (fader);

            syncButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.sync", cyan, true, ButtonIcon::sync);
            syncButtons[(size_t) d]->setTooltipKey ("tip.sync");
            syncButtons[(size_t) d]->onPress = [this, d] { if (onSync) onSync (d); };
            addAndMakeVisible (*syncButtons[(size_t) d]);

            rangeButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.range8", orange, false);
            rangeButtons[(size_t) d]->setTooltipKey ("tip.tempoRange");
            rangeButtons[(size_t) d]->onPress = [this, d]
            {
                rangeIndex[(size_t) d] = (rangeIndex[(size_t) d] + 1) % 3;
                static const char* keys[] = { "deck.range8", "deck.range16", "deck.range50" };
                rangeButtons[(size_t) d]->setLabelKey (keys[rangeIndex[(size_t) d]]);
                // El tempo no cambia: solo la escala del fader (más precisión con rango chico).
                const auto range = kTempoRanges[rangeIndex[(size_t) d]];
                if (std::abs (tempoPercent[(size_t) d]) > range)
                {
                    tempoPercent[(size_t) d] = juce::jlimit (-range, range, tempoPercent[(size_t) d]);
                    if (onTempo) onTempo (d, tempoPercent[(size_t) d]);
                }
                updateTempoFader (d);
            };
            addAndMakeVisible (*rangeButtons[(size_t) d]);
        }

        // --- Tiras de canal (Fase 2) --------------------------------------------------
        static const char* stripKeys[] = { "mixer.gain", "mixer.high", "mixer.mid", "mixer.low", "mixer.filter" };
        for (int d = 0; d < 2; ++d)
        {
            for (int k = 0; k < 5; ++k)
            {
                auto knob = std::make_unique<KbKnob> (stripKeys[k]);
                knob->setBipolar (true);
                knob->setDoubleClickReturnValue (true, 0.5);
                knob->setValue (0.5, juce::dontSendNotification);
                auto* raw = knob.get();
                knob->onValueChange = [this, d, k, raw]
                {
                    const auto v = (float) raw->getValue();
                    if (k == 0)      { if (onTrim)   onTrim (d, v); }
                    else if (k == 4) { if (onFilter) onFilter (d, v); }
                    else             { if (onEq)     onEq (d, 3 - k, v); }   // HIGH=2, MID=1, LOW=0
                };
                addAndMakeVisible (*knob);
                static const char* tips[] = { "tip.gain", "tip.high", "tip.mid", "tip.low", "tip.filter" };
                knob->setTooltipKey (tips[k]);
                stripKnobs[(size_t) d][(size_t) k] = std::move (knob);
            }

            auto cue = std::make_unique<GlowButton> ("mixer.cue", cyan, false);
            auto* rawCue = cue.get();
            cue->onPress = [this, d, rawCue]
            {
                rawCue->setLit (! rawCue->isLit());
                if (onChannelCue) onChannelCue (d, rawCue->isLit());
            };
            addAndMakeVisible (*cue);
            cue->setTooltipKey ("tip.channelCue");
            channelCueButtons[(size_t) d] = std::move (cue);

            channelMeters[(size_t) d] = std::make_unique<LevelMeter> (true, 16);
            channelMeters[(size_t) d]->setTooltipKey ("tip.channelMeter");
            addAndMakeVisible (*channelMeters[(size_t) d]);
        }

        phonesKnob.setTooltipKey ("tip.phones");
        cueMixKnob.setTooltipKey ("tip.cueMix");
        splitButton.setTooltipKey ("tip.split");
        phonesKnob.setDoubleClickReturnValue (true, 0.7);
        phonesKnob.onValueChange = [this] { if (onPhones) onPhones ((float) phonesKnob.getValue()); };
        cueMixKnob.setBipolar (true);
        cueMixKnob.setDoubleClickReturnValue (true, 0.0);
        cueMixKnob.onValueChange = [this] { if (onCueMix) onCueMix ((float) cueMixKnob.getValue()); };
        splitButton.onPress = [this]
        {
            splitButton.setLit (! splitButton.isLit());
            if (onSplit) onSplit (splitButton.isLit());
        };
        addAndMakeVisible (phonesKnob);
        addAndMakeVisible (cueMixKnob);
        addAndMakeVisible (splitButton);
    }

    void ControllerView::setTrim (int d, float v)   { if (juce::isPositiveAndBelow (d, 2)) stripKnobs[(size_t) d][0]->setValue (v, juce::dontSendNotification); }
    void ControllerView::setFilter (int d, float v) { if (juce::isPositiveAndBelow (d, 2)) stripKnobs[(size_t) d][4]->setValue (v, juce::dontSendNotification); }
    void ControllerView::setEq (int d, int band, float v)
    {
        if (juce::isPositiveAndBelow (d, 2) && juce::isPositiveAndBelow (band, 3))
            stripKnobs[(size_t) d][(size_t) (3 - band)]->setValue (v, juce::dontSendNotification);
    }
    void ControllerView::setChannelCue (int d, bool on) { if (juce::isPositiveAndBelow (d, 2)) channelCueButtons[(size_t) d]->setLit (on); }
    void ControllerView::setPhones (float v)  { phonesKnob.setValue (v, juce::dontSendNotification); }
    void ControllerView::setCueMix (float v)  { cueMixKnob.setValue (v, juce::dontSendNotification); }
    void ControllerView::setSplit (bool on)   { splitButton.setLit (on); }

    void ControllerView::setTempo (int d, float percent)
    {
        if (! juce::isPositiveAndBelow (d, 2) || tempoFaders[(size_t) d]->isMouseButtonDown())
            return;
        if (std::abs (percent - tempoPercent[(size_t) d]) < 0.0005f)
            return;

        tempoPercent[(size_t) d] = percent;
        // Si el SYNC pidió más que el rango actual, se agranda el rango.
        static const char* keys[] = { "deck.range8", "deck.range16", "deck.range50" };
        while (rangeIndex[(size_t) d] < 2 && std::abs (percent) > kTempoRanges[rangeIndex[(size_t) d]] + 1.0e-4f)
            rangeButtons[(size_t) d]->setLabelKey (keys[++rangeIndex[(size_t) d]]);
        updateTempoFader (d);
    }

    void ControllerView::updateTempoFader (int d)
    {
        const auto range = kTempoRanges[rangeIndex[(size_t) d]];
        tempoFaders[(size_t) d]->setValue (juce::jlimit (-1.0, 1.0, (double) (-tempoPercent[(size_t) d] / range)),
                                           juce::dontSendNotification);
    }

    void ControllerView::setSyncLit (int d, bool on)
    {
        if (juce::isPositiveAndBelow (d, 2))
            syncButtons[(size_t) d]->setLit (on);
    }

    void ControllerView::setPlatter (int d, double positionSeconds, bool playing, bool loaded)
    {
        if (juce::isPositiveAndBelow (d, 2))
            platters[(size_t) d]->setState (positionSeconds, playing, loaded);
    }

    juce::String ControllerView::getTooltip()
    {
        const auto p = getMouseXYRelative().toFloat();
        for (const auto& L : deckLayouts)
        {
            if (L.platter.contains (p))
                return tr ("tip.platter");
            for (const auto& pad : L.pads)
                if (pad.contains (p))
                    return tr ("tip.pads");
            for (const auto& m : L.modes)
                if (m.contains (p))
                    return tr ("tip.padModes");
        }
        return {};
    }

    void ControllerView::invalidateBackground()
    {
        background = {};
        repaint();
    }

    void ControllerView::pushChannelMeter (int d, const engine::MeterSource::Reading& r, float dt)
    {
        if (juce::isPositiveAndBelow (d, 2))
            channelMeters[(size_t) d]->push (r, dt);
    }

    // =========================================================================
    //  Estado
    // =========================================================================
    void ControllerView::setDeckLights (int deck, const DeckLights& l, bool blinkPhase)
    {
        if (! juce::isPositiveAndBelow (deck, 2))
            return;

        // PLAY: encendido reproduciendo; titila en pausa con pista cargada (CDJ).
        playButtons[(size_t) deck]->setLit (l.playing && ! l.cuePreview);
        // PAUSA: titila con una pista cargada en pausa (encendido fijo durante la preescucha de CUE: no).
        pauseButtons[(size_t) deck]->setLit (l.loaded && ! l.playing && blinkPhase);
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
        background = {};

        for (int d = 0; d < 2; ++d)
        {
            const auto& dl = deckLayouts[(size_t) d];
            platters[(size_t) d]->setBounds (dl.platter.toNearestInt());
            tempoFaders[(size_t) d]->setBounds (dl.tempoSlot.reduced (unit * 0.8f, unit * 1.0f).toNearestInt());
            syncButtons[(size_t) d]->setBounds (dl.sync.expanded (dl.sync.getHeight() * 0.08f).toNearestInt());
            rangeButtons[(size_t) d]->setBounds (dl.tempoRange.toNearestInt());
            rangeButtons[(size_t) d]->setTextScale (0.5f);
        }

        for (int d = 0; d < 2; ++d)
        {
            const auto& dl = deckLayouts[(size_t) d];
            playButtons[(size_t) d]->setBounds (dl.play.expanded (dl.play.getHeight() * 0.08f).toNearestInt());
            pauseButtons[(size_t) d]->setBounds (dl.pause.expanded (dl.pause.getHeight() * 0.08f).toNearestInt());
            cueButtons[(size_t) d]->setBounds (dl.cue.expanded (dl.cue.getHeight() * 0.08f).toNearestInt());
            volumeFaders[(size_t) d]->setBounds (mixer.faders[(size_t) d].toNearestInt());
        }

        crossfader.setBounds (mixer.crossfader.toNearestInt());
        masterKnob.setGeometry (mixer.masterRadius, unit * 2.6f * 0.8f);
        masterKnob.setBounds (masterKnob.boundsForCentre (mixer.masterCentre).toNearestInt());

        const auto labelH = unit * 2.6f * 0.8f;
        for (int d = 0; d < 2; ++d)
        {
            for (int k = 0; k < 5; ++k)
            {
                auto& knob = *stripKnobs[(size_t) d][(size_t) k];
                knob.setGeometry (mixer.eqRadius, labelH);
                knob.setBounds (knob.boundsForCentre (mixer.eqCentres[(size_t) d][(size_t) k]).toNearestInt());
            }
            channelCueButtons[(size_t) d]->setBounds (mixer.cueButtons[(size_t) d].expanded (mixer.cueButtons[(size_t) d].getHeight() * 0.1f).toNearestInt());
            channelCueButtons[(size_t) d]->setTextScale (0.5f);
        }
        channelMeters[0]->setBounds (mixer.meterL.toNearestInt());
        channelMeters[1]->setBounds (mixer.meterR.toNearestInt());

        for (auto* k : { &phonesKnob, &cueMixKnob })
            k->setGeometry (mixer.smallRadius, unit * 2.6f * 0.7f);
        phonesKnob.setBounds (phonesKnob.boundsForCentre (mixer.phonesCentre).toNearestInt());
        cueMixKnob.setBounds (cueMixKnob.boundsForCentre (mixer.cueMixCentre).toNearestInt());
        splitButton.setBounds (mixer.split.toNearestInt());
        splitButton.setTextScale (0.5f);
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
            L.tempoRange = tempoCol.removeFromBottom (unit * 6.5f).reduced (0.0f, unit * 0.3f);
            L.tempoSlot  = tempoCol.reduced (0.0f, unit * 1.0f);

            L.header = r.removeFromTop (textH * 1.4f);

            auto platterArea = r.removeFromTop (r.getHeight() * 0.66f);
            const auto diameter = juce::jmin (platterArea.getWidth(), platterArea.getHeight()) * 0.96f;
            L.platter = Rectangle<float> (diameter, diameter).withCentre (platterArea.getCentre());

            r.removeFromTop (unit * 2.0f);
            auto transport = L.mirrored ? r.removeFromRight (r.getWidth() * 0.44f) : r.removeFromLeft (r.getWidth() * 0.44f);
            if (L.mirrored) r.removeFromRight (unit * 2.0f); else r.removeFromLeft (unit * 2.0f);

            const auto btnD  = juce::jmin (transport.getWidth() / 4.2f, transport.getHeight() * 0.9f);
            const auto cellW = transport.getWidth() / 4.0f;
            Rectangle<float>* cells[] = { &L.sync, &L.cue, &L.pause, &L.play };
            for (int i = 0; i < 4; ++i)
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

        auto phonesArea = centre.removeFromBottom (centre.getHeight() * 0.36f);
        auto splitArea  = phonesArea.removeFromBottom (phonesArea.getHeight() * 0.3f);
        M.split = splitArea.withSizeKeepingCentre (juce::jmin (splitArea.getWidth() * 0.6f, unit * 12.0f),
                                                   juce::jmin (splitArea.getHeight() * 0.8f, unit * 4.5f));
        M.smallRadius  = juce::jmin (phonesArea.getHeight() * 0.24f, centre.getWidth() * 0.13f);
        M.phonesCentre = { phonesArea.getX() + phonesArea.getWidth() * 0.27f, phonesArea.getCentreY() - unit * 1.2f };
        M.cueMixCentre = { phonesArea.getX() + phonesArea.getWidth() * 0.73f, phonesArea.getCentreY() - unit * 1.2f };

        auto meters = centre.reduced (centre.getWidth() * 0.28f, unit * 1.0f).withTrimmedBottom (unit * 3.2f);
        const auto mW = meters.getWidth() * 0.4f;
        M.meterL = meters.withWidth (mW);
        M.meterR = meters.withTrimmedLeft (meters.getWidth() - mW);
    }

    // =========================================================================
    //  Pintado de las partes estáticas
    // =========================================================================
    void ControllerView::paint (Graphics& g)
    {
        const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (background.isNull() || ! juce::exactlyEqual (scale, backgroundScale))
            renderBackground (scale);

        g.drawImage (background, getLocalBounds().toFloat());
        paintDropOverlay (g);
    }

    void ControllerView::renderBackground (float scale)
    {
        backgroundScale = scale;
        background = juce::Image (juce::Image::RGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                                  juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), false);
        juce::Graphics g (background);
        g.addTransform (juce::AffineTransform::scale (scale));
        paintBackground (g);
    }

    void ControllerView::paintBackground (Graphics& g) const
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

    void ControllerView::paintDropOverlay (Graphics& g) const
    {
        if (! juce::isPositiveAndBelow (dropTarget, 2))
            return;
        const auto& L = deckLayouts[(size_t) dropTarget];
        g.setColour (cyan.withAlpha (0.18f));
        g.fillRoundedRectangle (L.area.expanded (unit), unit * 2.0f);
        g.setColour (cyan);
        g.drawRoundedRectangle (L.area.expanded (unit), unit * 2.0f, juce::jmax (2.0f, unit * 0.6f));
    }

    void ControllerView::paintDeck (Graphics& g, const DeckLayout& L, int deckIndex) const
    {
        const auto textH  = unit * 3.2f;
        const auto accent = deckColour (deckIndex);

        // TEMPO (Fase 3)
        draw::text (g, tr ("deck.tempo"), L.tempoLabel, textH * 0.85f, textOnChassis);
        g.setColour (panelDark.withAlpha (0.85f));
        g.fillRoundedRectangle (L.tempoSlot, unit * 1.2f);
        // Marcas de "-" arriba y "+" abajo (el fader es un componente).
        draw::text (g, "-", L.tempoSlot.withHeight (unit * 3.0f).translated (0.0f, -unit * 0.2f), unit * 3.0f, textSecondary);
        draw::text (g, "+", L.tempoSlot.withTrimmedTop (L.tempoSlot.getHeight() - unit * 3.0f), unit * 3.0f, textSecondary);

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

        juce::ignoreUnused (diameter, c, accent);
        // Anillo, vinilo y etiqueta giratoria: PlatterView.

        // SYNC, PLAY y CUE son componentes.

        // Modos de pads y pads (Fase 4)
        const char* modeKeys[] = { "deck.mode.hotcue", "deck.mode.fx", "deck.mode.sampler", "deck.mode.loop" };
        for (int i = 0; i < 4; ++i)
            draw::button (g, L.modes[(size_t) i], orange, tr (modeKeys[i]), i == 0, false, textH * 0.62f);
        for (int i = 0; i < 4; ++i)
            draw::button (g, L.pads[(size_t) i], cyan, juce::String (i + 1), false, false, L.pads[(size_t) i].getHeight() * 0.32f);

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

        // GAIN, EQ, FILTER, CUE, PHONES, CUE MIX, SPLIT y vúmetros son componentes.
        // Etiquetas de los vúmetros de canal.
        draw::text (g, "1", M.meterL.withY (M.meterL.getBottom() + unit * 0.3f).withHeight (textH), textH * 0.8f, textSecondary);
        draw::text (g, "2", M.meterR.withY (M.meterR.getBottom() + unit * 0.3f).withHeight (textH), textH * 0.8f, textSecondary);
    }
} // namespace kb::ui
