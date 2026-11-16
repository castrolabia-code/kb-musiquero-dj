#include "ui/ControllerView.h"
#include "ui/KbLookAndFeel.h"
#include "ui/SkinAssets.h"

#include <cmath>

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/Palette.h"
#include "ui/TribalArt.h"

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
        setOpaque (false);   // el chasis amarillo lo pinta la ventana

        for (int d = 0; d < 2; ++d)
        {
            // Transporte rectangular con borde de neón (como la referencia).
            playButtons[(size_t) d]  = std::make_unique<GlowButton> ("deck.play", play, false, ButtonIcon::play);
            pauseButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.pause", orange, false, ButtonIcon::pause);
            cueButtons[(size_t) d]   = std::make_unique<GlowButton> ("deck.cue", alert, false, ButtonIcon::cue);
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

        // --- AUTO MIX ----------------------------------------------------------------------
        autoMixButton.setTooltipKey ("tip.autoMix");
        autoMixButton.onPress = [this] { if (onAutoMix) onAutoMix(); };
        addAndMakeVisible (autoMixButton);

        mixTimeKnob.setTooltipKey ("tip.mixTime");
        mixTimeKnob.setLabelFormatter ([] (double v)
        {
            return tr ("mixer.mixTime").replace ("{s}", juce::String (juce::roundToInt (knobToMixSeconds (v))));
        });
        mixTimeKnob.setDoubleClickReturnValue (true, mixSecondsToKnob (10.0));
        mixTimeKnob.setValue (mixSecondsToKnob (10.0), juce::dontSendNotification);
        mixTimeKnob.onValueChange = [this] { if (onAutoMixTime) onAutoMixTime (knobToMixSeconds (mixTimeKnob.getValue())); };
        addAndMakeVisible (mixTimeKnob);

        masterKnob.setOnChassis (true);
        phonesKnob.setOnChassis (true);
        masterKnob.setDoubleClickReturnValue (true, 0.85);
        masterKnob.onValueChange = [this] { if (onMaster) onMaster ((float) masterKnob.getValue()); };
        addAndMakeVisible (masterKnob);

        // --- Tempo, SYNC y platos (Fase 3) -------------------------------------------------
        for (int d = 0; d < 2; ++d)
        {
            platters[(size_t) d] = std::make_unique<PlatterView> (d);
            addAndMakeVisible (*platters[(size_t) d]);

            auto fader = std::make_unique<KbFader> (true);
            fader->setThinStyle (true);
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

            syncButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.sync", cyan, false, ButtonIcon::sync);
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

            // Key Lock y key shift
            keyLockButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.keyLock", orange, false);
            keyDownButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.keyDown", cyan, false);
            keyUpButtons[(size_t) d]   = std::make_unique<GlowButton> ("deck.keyUp", cyan, false);
            keyLockButtons[(size_t) d]->setTooltipKey ("tip.keyLock");
            keyDownButtons[(size_t) d]->setTooltipKey ("tip.keyShift");
            keyUpButtons[(size_t) d]->setTooltipKey ("tip.keyShift");
            keyLockButtons[(size_t) d]->onPress = [this, d] { if (onKeyLock) onKeyLock (d); };
            keyDownButtons[(size_t) d]->onPress = [this, d] { if (onKeyShift) onKeyShift (d, -1); };
            keyUpButtons[(size_t) d]->onPress   = [this, d] { if (onKeyShift) onKeyShift (d, +1); };
            keyDownButtons[(size_t) d]->onDoubleClick = [this, d] { if (onKeyShift) onKeyShift (d, 0); };
            keyUpButtons[(size_t) d]->onDoubleClick   = [this, d] { if (onKeyShift) onKeyShift (d, 0); };
            for (auto* b : { keyLockButtons[(size_t) d].get(), keyDownButtons[(size_t) d].get(), keyUpButtons[(size_t) d].get() })
            {
                b->setTextScale (0.6f);
                addAndMakeVisible (*b);
            }
        }

        // --- Pads, modos, SHIFT / SLIP / VINYL y plato (Fase 4) ------------------------------
        static const char* modeKeys[] = { "deck.mode.hotcue", "deck.mode.fx", "deck.mode.sampler", "deck.mode.loop" };
        for (int d = 0; d < 2; ++d)
        {
            for (int i = 0; i < 4; ++i)
            {
                auto mb = std::make_unique<GlowButton> (modeKeys[i], orange, false);
                mb->setTooltipKey ("tip.padModes");
                mb->onPress = [this, d, i] { if (onPadMode) onPadMode (d, i); };
                addAndMakeVisible (*mb);
                modeButtons[(size_t) d][(size_t) i] = std::move (mb);

                auto pb = std::make_unique<GlowButton> (juce::String(), cyan, false);
                pb->setPadStyle (true);
                pb->setLabelText (juce::String (i + 1));
                pb->setTooltipKey ("tip.pads");
                pb->onPress   = [this, d, i] { if (onPad) onPad (d, i, true); };
                pb->onRelease = [this, d, i] { if (onPad) onPad (d, i, false); };
                auto* raw = pb.get();
                pb->onRightClick = [this, d, i, raw] { if (onPadMenu) onPadMenu (d, i, raw); };
                addAndMakeVisible (*pb);
                padButtons[(size_t) d][(size_t) i] = std::move (pb);
            }

            shiftButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.shift", juce::Colours::white, false);
            shiftButtons[(size_t) d]->setTooltipKey ("tip.shift");
            shiftButtons[(size_t) d]->onPress   = [this, d] { if (onShift) onShift (d, true); };
            shiftButtons[(size_t) d]->onRelease = [this, d] { if (onShift) onShift (d, false); };
            slipButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.slip", cyan, false);
            slipButtons[(size_t) d]->setTooltipKey ("tip.slip");
            slipButtons[(size_t) d]->onPress = [this, d] { if (onSlip) onSlip (d); };
            vinylButtons[(size_t) d] = std::make_unique<GlowButton> ("deck.vinyl", orange, false);
            vinylButtons[(size_t) d]->setTooltipKey ("tip.vinyl");
            vinylButtons[(size_t) d]->onPress = [this, d] { if (onVinyl) onVinyl (d); };
            for (auto* b : { shiftButtons[(size_t) d].get(), slipButtons[(size_t) d].get(), vinylButtons[(size_t) d].get() })
            {
                b->setTextScale (0.56f);
                addAndMakeVisible (*b);
            }

            auto& pv = *platters[(size_t) d];
            pv.onTouch = [this, d] (bool t) { if (onJogTouch) onJogTouch (d, t); };
            pv.onTurn  = [this, d] (double turns) { if (onJogTurn) onJogTurn (d, turns); };
            pv.onWheel = [this, d] (float w) { if (onJogWheel) onJogWheel (d, w); };
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

            auto cue = std::make_unique<GlowButton> ("mixer.cue", cyan, false, ButtonIcon::headphones);
            cue->setLabelText ({});   // solo el ícono de auriculares; "CUE" va impreso debajo
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

    void ControllerView::setKeyState (int d, bool keyLock, int keyShift)
    {
        if (! juce::isPositiveAndBelow (d, 2))
            return;
        keyLockButtons[(size_t) d]->setLit (keyLock);
        keyDownButtons[(size_t) d]->setLit (keyShift < 0);
        keyUpButtons[(size_t) d]->setLit (keyShift > 0);
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

    void ControllerView::setPlatterArtwork (int d, const juce::Image& image)
    {
        if (juce::isPositiveAndBelow (d, 2))
            platters[(size_t) d]->setArtwork (image);
    }

    void ControllerView::cycleTempoRange (int d)
    {
        if (juce::isPositiveAndBelow (d, 2) && rangeButtons[(size_t) d]->onPress)
            rangeButtons[(size_t) d]->onPress();
    }

    void ControllerView::setPlatterBeatGrid (int d, double bpm, double first)
    {
        if (juce::isPositiveAndBelow (d, 2))
            platters[(size_t) d]->setBeatGrid (bpm, first);
    }

    juce::String ControllerView::getTooltip()
    {
        const auto p = getMouseXYRelative().toFloat();
        for (const auto& L : deckLayouts)
        {
            if (L.platter.contains (p))
                return tr ("tip.platter");
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
    void ControllerView::assignMidiIds()
    {
        // Identificadores de función MIDI (click derecho -> MIDI Learn).
        for (int d = 0; d < 2; ++d)
        {
            const auto p = "deck" + juce::String (d + 1) + ".";
            playButtons[(size_t) d]->setComponentID (p + "play");
            pauseButtons[(size_t) d]->setComponentID (p + "pause");
            cueButtons[(size_t) d]->setComponentID (p + "cue");
            syncButtons[(size_t) d]->setComponentID (p + "sync");
            keyLockButtons[(size_t) d]->setComponentID (p + "keyLock");
            keyDownButtons[(size_t) d]->setComponentID (p + "keyDown");
            keyUpButtons[(size_t) d]->setComponentID (p + "keyUp");
            slipButtons[(size_t) d]->setComponentID (p + "slip");
            vinylButtons[(size_t) d]->setComponentID (p + "vinyl");
            shiftButtons[(size_t) d]->setComponentID (p + "shift");
            rangeButtons[(size_t) d]->setComponentID (p + "tempoRange");
            tempoFaders[(size_t) d]->setComponentID (p + "tempo");
            platters[(size_t) d]->setComponentID (p + "jog");
            static const char* modes[] = { "modeHotCue", "modeFx", "modeSampler", "modeLoop" };
            for (int i = 0; i < 4; ++i)
            {
                modeButtons[(size_t) d][(size_t) i]->setComponentID (p + modes[i]);
                padButtons[(size_t) d][(size_t) i]->setComponentID (p + "pad" + juce::String (i + 1));
            }
            const auto c = "mixer.ch" + juce::String (d + 1) + ".";
            volumeFaders[(size_t) d]->setComponentID (c + "volume");
            static const char* strip[] = { "gain", "high", "mid", "low", "filter" };
            for (int k = 0; k < 5; ++k)
                stripKnobs[(size_t) d][(size_t) k]->setComponentID (c + strip[k]);
            channelCueButtons[(size_t) d]->setComponentID (c + "cue");
        }
        crossfader.setComponentID ("mixer.crossfader");
        masterKnob.setComponentID ("mixer.master");
        phonesKnob.setComponentID ("mixer.phones");
        cueMixKnob.setComponentID ("mixer.cueMix");
        splitButton.setComponentID ("mixer.split");
        autoMixButton.setComponentID ("mixer.autoMix");
        mixTimeKnob.setComponentID ("mixer.mixTime");
    }

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

    void ControllerView::setCrossfader (float v)
    {
        // El AUTO MIX lo mueve solo: se sigue al motor, salvo mientras el usuario lo arrastra.
        if (! crossfader.isMouseButtonDown())
            crossfader.setValue (v, juce::dontSendNotification);
    }

    // Perilla MIX logarítmica: 2 s .. 60 s (mitad de recorrido ≈ 11 s).
    double ControllerView::knobToMixSeconds (double v)
    {
        return kAutoMixMinSeconds * std::pow (kAutoMixMaxSeconds / kAutoMixMinSeconds, juce::jlimit (0.0, 1.0, v));
    }

    double ControllerView::mixSecondsToKnob (double seconds)
    {
        const auto s = juce::jlimit (kAutoMixMinSeconds, kAutoMixMaxSeconds, seconds);
        return std::log (s / kAutoMixMinSeconds) / std::log (kAutoMixMaxSeconds / kAutoMixMinSeconds);
    }

    void ControllerView::setAutoMixSeconds (double seconds)
    {
        mixTimeKnob.setValue (mixSecondsToKnob (seconds), juce::dontSendNotification);
    }

    void ControllerView::setAutoMixState (float progress)
    {
        autoMixButton.setLit (progress >= 0.0f);
    }
    void ControllerView::setMaster (float v)     { masterKnob.setValue (v, juce::dontSendNotification); }

    void ControllerView::setPadView (int d, const PadView& v)
    {
        if (! juce::isPositiveAndBelow (d, 2))
            return;
        static const char* primaryKeys[]   = { "deck.mode.hotcue",  "deck.mode.fx",   "deck.mode.sampler",  "deck.mode.loop" };
        static const char* secondaryKeys[] = { "deck.mode.hotcue2", "deck.mode.roll", "deck.mode.sampler2", "deck.mode.loopManual" };
        for (int i = 0; i < 4; ++i)
        {
            auto& mb = *modeButtons[(size_t) d][(size_t) i];
            // Con SHIFT apretado los botones muestran su modo secundario.
            const bool showSecondary = v.shift || (i == v.group && v.secondary);
            mb.setLabelKey (showSecondary ? secondaryKeys[i] : primaryKeys[i]);
            mb.setLit (i == v.group);

            auto& pb = *padButtons[(size_t) d][(size_t) i];
            const auto& text = v.labels[(size_t) i];
            pb.setLabelText (text);
            // Letra según el largo: números grandes, palabras y nombres de samples más chicos.
            const auto len = (float) juce::jmax (1, text.length());
            pb.setTextScale (len <= 2.0f ? 0.42f : len <= 6.0f ? 0.3f : juce::jmax (0.15f, 0.3f * 6.0f / len));
            pb.setColour (v.colours[(size_t) i]);
            pb.setLit (v.lit[(size_t) i]);
            pb.setProgress (v.progress[(size_t) i]);
        }
        padGroup[(size_t) d] = v.group;
        padSecondary[(size_t) d] = v.secondary;
        shiftButtons[(size_t) d]->setLit (v.shift);
        slipButtons[(size_t) d]->setLit (v.slip);
        vinylButtons[(size_t) d]->setLit (v.vinyl);
    }

    int ControllerView::padAt (juce::Point<int> p, int& padIndex) const
    {
        for (int d = 0; d < 2; ++d)
            for (int i = 0; i < 4; ++i)
                if (deckLayouts[(size_t) d].pads[(size_t) i].contains (p.toFloat()))
                {
                    padIndex = i;
                    return d;
                }
        padIndex = -1;
        return -1;
    }

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
        if (getComponentID().isEmpty())   // una sola vez (todos los controles ya existen)
        {
            setComponentID ("controller");
            assignMidiIds();
        }
        computeLayout();
        background = {};

        for (int d = 0; d < 2; ++d)
        {
            const auto& dl = deckLayouts[(size_t) d];
            platters[(size_t) d]->setBounds (dl.platter.toNearestInt());
            tempoFaders[(size_t) d]->setBounds (dl.tempoSlot.withSizeKeepingCentre (dl.tempoSlot.getWidth() * 0.6f, dl.tempoSlot.getHeight()).toNearestInt());
            syncButtons[(size_t) d]->setBounds (dl.sync.toNearestInt());
            syncButtons[(size_t) d]->setTextScale (0.34f);
            rangeButtons[(size_t) d]->setBounds (dl.tempoRange.toNearestInt());
            keyLockButtons[(size_t) d]->setBounds (dl.keyLock.toNearestInt());
            keyDownButtons[(size_t) d]->setBounds (dl.keyDown.toNearestInt());
            keyUpButtons[(size_t) d]->setBounds (dl.keyUp.toNearestInt());
            shiftButtons[(size_t) d]->setBounds (dl.shift.toNearestInt());
            slipButtons[(size_t) d]->setBounds (dl.slip.toNearestInt());
            vinylButtons[(size_t) d]->setBounds (dl.vinyl.toNearestInt());
            for (int i = 0; i < 4; ++i)
            {
                modeButtons[(size_t) d][(size_t) i]->setBounds (dl.modes[(size_t) i].expanded (unit * 0.3f).toNearestInt());
                modeButtons[(size_t) d][(size_t) i]->setTextScale (i == 1 || i == 3 ? 0.5f : 0.46f);
                padButtons[(size_t) d][(size_t) i]->setBounds (dl.pads[(size_t) i].expanded (unit * 0.3f).toNearestInt());
            }
            rangeButtons[(size_t) d]->setTextScale (0.62f);
        }

        for (int d = 0; d < 2; ++d)
        {
            const auto& dl = deckLayouts[(size_t) d];
            playButtons[(size_t) d]->setBounds (dl.play.toNearestInt());
            pauseButtons[(size_t) d]->setBounds (dl.pause.toNearestInt());
            cueButtons[(size_t) d]->setBounds (dl.cue.toNearestInt());
            for (auto* b : { playButtons[(size_t) d].get(), pauseButtons[(size_t) d].get(), cueButtons[(size_t) d].get() })
                b->setTextScale (0.34f);
            volumeFaders[(size_t) d]->setBounds (mixer.faders[(size_t) d].toNearestInt());
        }

        crossfader.setBounds (mixer.crossfader.toNearestInt());
        autoMixButton.setBounds (mixer.autoMix.toNearestInt());
        autoMixButton.setTextScale (0.56f);
        mixTimeKnob.setGeometry (mixer.mixTimeRadius, unit * 3.0f);
        mixTimeKnob.setBounds (mixTimeKnob.boundsForCentre (mixer.mixTimeCentre).toNearestInt());
        masterKnob.setGeometry (mixer.masterRadius, unit * 3.0f);
        masterKnob.setBounds (masterKnob.boundsForCentre (mixer.masterCentre).toNearestInt());

        const auto labelH = unit * 2.6f;
        for (int d = 0; d < 2; ++d)
        {
            for (int k = 0; k < 5; ++k)
            {
                auto& knob = *stripKnobs[(size_t) d][(size_t) k];
                knob.setGeometry (mixer.eqRadius, labelH);
                knob.setLabelSide (d == 0 ? -1 : +1);   // nombre hacia afuera del mixer
                knob.setBounds (knob.boundsForCentre (mixer.eqCentres[(size_t) d][(size_t) k]).toNearestInt());
            }
            channelCueButtons[(size_t) d]->setBounds (mixer.cueButtons[(size_t) d].toNearestInt());
            channelCueButtons[(size_t) d]->setTextScale (0.62f);
        }
        channelMeters[0]->setBounds (mixer.meterL.toNearestInt());
        channelMeters[1]->setBounds (mixer.meterR.toNearestInt());

        phonesKnob.setGeometry (deckLayouts[1].sideKnobRadius, unit * 3.0f);
        cueMixKnob.setGeometry (mixer.smallRadius, unit * 2.4f);
        phonesKnob.setBounds (phonesKnob.boundsForCentre (mixer.phonesCentre).toNearestInt());
        cueMixKnob.setBounds (cueMixKnob.boundsForCentre (mixer.cueMixCentre).toNearestInt());
        splitButton.setBounds (mixer.split.toNearestInt());
        splitButton.setTextScale (0.62f);
    }

    void ControllerView::computeLayout()
    {
        // Layout de la imagen de referencia (deck 1; el deck 2 es su espejo):
        //
        //   [regla de TEMPO] [  plato grande  "(1)" ] [CUE  SHIFT VINYL | MASTER ]
        //                                             [HOT CUE] [SAMPLER]
        //                                             [  FX   ] [ LOOP  ]
        //                                             [pad 1  ] [pad 2  ]
        //   [SYNC] [CUE] [PAUSA] [PLAY]               [pad 3  ] [pad 4  ]
        auto b = getLocalBounds().toFloat();
        unit = b.getHeight() / 100.0f;
        chassis = b;

        auto inner = b.reduced (unit * 1.0f, unit * 1.5f);
        const auto gap   = unit * 2.5f;
        const auto deckW = inner.getWidth() * 0.385f;

        const auto deck1 = inner.removeFromLeft (deckW);
        inner.removeFromLeft (gap);
        const auto deck2 = inner.removeFromRight (deckW);
        inner.removeFromRight (gap);

        for (int d = 0; d < 2; ++d)
        {
            auto& L = deckLayouts[(size_t) d];
            L.mirrored = d == 1;
            L.area = d == 0 ? deck1 : deck2;
            auto r = L.area;
            auto takeOuter = [&L] (Rectangle<float>& from, float w) { return L.mirrored ? from.removeFromRight (w) : from.removeFromLeft (w); };
            auto takeInner = [&L] (Rectangle<float>& from, float w) { return L.mirrored ? from.removeFromLeft (w) : from.removeFromRight (w); };

            // Columna lateral (pads) del lado del mixer.
            auto side = takeInner (r, r.getWidth() * 0.39f);
            takeInner (r, unit * 2.0f);

            // Fila de transporte abajo, a lo ancho de la regla de tempo + el plato.
            auto transport = r.removeFromBottom (unit * 12.0f);
            r.removeFromBottom (unit * 1.0f);

            // Regla de TEMPO.
            auto tempoCol = takeOuter (r, r.getWidth() * 0.15f);
            takeOuter (r, unit * 1.0f);
            L.tempoRange = tempoCol.removeFromTop (unit * 5.0f).reduced (unit * 0.6f, unit * 0.3f);
            L.tempoLabel = tempoCol.removeFromBottom (unit * 4.0f);
            L.tempoSlot  = tempoCol.reduced (0.0f, unit * 1.0f);

            // Encima del plato: KEY LOCK, KEY -, KEY +, SLIP (chicos) y el número "(1)".
            auto top = r.removeFromTop (unit * 6.0f);
            L.number = takeInner (top, unit * 12.0f);
            {
                auto row = top.withSizeKeepingCentre (top.getWidth(), unit * 4.6f);
                const auto g2 = unit * 0.7f;
                auto put = [&] (float w) { auto x = takeOuter (row, w); takeOuter (row, g2); return x; };
                L.keyLock = put (unit * 14.0f);
                L.keyDown = put (unit * 10.0f);
                L.keyUp   = put (unit * 10.0f);
                L.slip    = put (unit * 10.0f);
            }
            const auto diameter = juce::jmin (r.getWidth(), r.getHeight());
            L.platter = Rectangle<float> (diameter, diameter).withCentre (r.getCentre());
            L.header = {};
            L.artInner = L.artOuter = L.artHeader = {};

            // SYNC  CUE  PAUSA  PLAY
            {
                const auto cellW = transport.getWidth() / 4.0f;
                Rectangle<float>* cells[] = { &L.sync, &L.cue, &L.pause, &L.play };
                for (int i = 0; i < 4; ++i)
                {
                    const auto idx = L.mirrored ? 3 - i : i;   // en el deck 2, PLAY queda del lado de afuera
                    juce::ignoreUnused (idx);
                    auto cell = transport.withWidth (cellW).withX (transport.getX() + cellW * (float) i);
                    *cells[L.mirrored ? i : i] = cell.reduced (unit * 0.9f, unit * 1.0f);
                }
            }

            // Columna lateral: [CUE / SHIFT / VINYL | perilla], modos y pads 2x2.
            auto head = side.removeFromTop (side.getHeight() * 0.27f);
            auto buttons = L.mirrored ? head.removeFromRight (head.getWidth() * 0.46f) : head.removeFromLeft (head.getWidth() * 0.46f);
            L.chCue      = buttons.removeFromTop (buttons.getHeight() * 0.36f).reduced (unit * 0.3f, unit * 0.2f);
            L.chCueLabel = buttons.removeFromTop (unit * 3.2f);
            auto pills = buttons.reduced (unit * 0.3f, unit * 0.2f);
            L.shift = pills.removeFromTop (pills.getHeight() * 0.5f).reduced (0.0f, unit * 0.35f);
            L.vinyl = pills.reduced (0.0f, unit * 0.35f);
            L.sideKnobRadius = juce::jmin (head.getWidth() * 0.2f, head.getHeight() * 0.26f);
            L.sideKnobCentre = { head.getCentreX(), head.getY() + L.sideKnobRadius * 1.32f + unit * 0.8f };

            side.removeFromTop (unit * 1.0f);
            auto modes = side.removeFromTop (unit * 12.5f);
            const auto colW = modes.getWidth() / 2.0f;
            auto modeRow1 = modes.removeFromTop (unit * 7.0f);
            auto modeRow2 = modes.reduced (0.0f, unit * 0.4f);
            // Botones de modo: 0 HOT CUE, 1 FX, 2 SAMPLER, 3 LOOP (FX y LOOP abajo, más chicos).
            L.modes[0] = modeRow1.withWidth (colW).reduced (unit * 0.6f, 0.0f);
            L.modes[2] = modeRow1.withTrimmedLeft (colW).reduced (unit * 0.6f, 0.0f);
            L.modes[1] = modeRow2.withWidth (colW).reduced (unit * 2.6f, 0.0f);
            L.modes[3] = modeRow2.withTrimmedLeft (colW).reduced (unit * 2.6f, 0.0f);

            side.removeFromTop (unit * 0.6f);
            const auto padSide = juce::jmin (side.getWidth() / 2.0f, side.getHeight() / 2.0f);
            const auto grid = Rectangle<float> (padSide * 2.0f, padSide * 2.0f).withCentre (side.getCentre());
            for (int i = 0; i < 4; ++i)
                L.pads[(size_t) i] = Rectangle<float> (padSide, padSide)
                                         .withPosition (grid.getX() + padSide * (float) (i % 2), grid.getY() + padSide * (float) (i / 2))
                                         .reduced (unit * 0.9f);
        }

        // --- Mixer (centro) -----------------------------------------------------------------
        auto& M = mixer;
        M.panel = inner;
        auto area = inner.reduced (unit * 1.5f);
        M.title = area.removeFromTop (unit * 5.0f);

        // MASTER (deck 1) y HEADPHONES (deck 2) van en las columnas laterales, como en la referencia.
        M.masterCentre = deckLayouts[0].sideKnobCentre;
        M.phonesCentre = deckLayouts[1].sideKnobCentre;
        M.masterRadius = deckLayouts[0].sideKnobRadius;
        M.cueButtons[0] = deckLayouts[0].chCue;
        M.cueButtons[1] = deckLayouts[1].chCue;

        auto xfaderArea = area.removeFromBottom (unit * 15.5f);
        // [AUTO MIX] [==== crossfader ====] [perilla MIX]
        auto amArea = xfaderArea.removeFromLeft (xfaderArea.getWidth() * 0.25f);
        auto mtArea = xfaderArea.removeFromRight (xfaderArea.getWidth() * 0.28f);
        M.autoMix = amArea.withSizeKeepingCentre (amArea.getWidth() * 0.96f, juce::jmin (amArea.getHeight() * 0.62f, unit * 7.5f));
        M.crossfader = xfaderArea.reduced (unit * 1.2f, unit * 1.2f);
        M.mixTimeRadius = juce::jmin (mtArea.getHeight() * 0.2f, mtArea.getWidth() * 0.2f) * 1.4f;
        M.mixTimeCentre = { mtArea.getCentreX(), mtArea.getY() + M.mixTimeRadius * 1.32f + unit * 0.6f };

        const auto stripW = area.getWidth() * 0.35f;
        Rectangle<float> strips[2] = { area.removeFromLeft (stripW), area.removeFromRight (stripW) };
        auto centre = area;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto s = strips[ch];
            auto knobs = s.removeFromTop (s.getHeight() * 0.68f);
            const auto rowH = knobs.getHeight() / 5.0f;
            M.eqRadius = juce::jmin (rowH * 0.21f, s.getWidth() * 0.2f) * 1.4f;   // 40 % más grandes
            for (int i = 0; i < 5; ++i)
                M.eqCentres[(size_t) ch][(size_t) i] = { knobs.getX() + knobs.getWidth() * (ch == 0 ? 0.68f : 0.32f),
                                                         knobs.getY() + rowH * ((float) i + 0.5f) };
            M.faders[(size_t) ch] = s.reduced (s.getWidth() * 0.3f, 0.0f).withTrimmedTop (unit * 1.2f);
        }

        auto bottom = centre.removeFromBottom (centre.getHeight() * 0.36f);
        auto splitArea = bottom.removeFromBottom (bottom.getHeight() * 0.32f);
        M.split = splitArea.withSizeKeepingCentre (juce::jmin (splitArea.getWidth() * 0.8f, unit * 12.0f),
                                                   juce::jmin (splitArea.getHeight() * 0.85f, unit * 4.5f));
        M.smallRadius  = juce::jmin (bottom.getHeight() * 0.22f, centre.getWidth() * 0.2f);
        M.cueMixCentre = { bottom.getCentreX(), bottom.getY() + M.smallRadius * 1.32f + unit * 0.6f };

        auto meters = centre.reduced (centre.getWidth() * 0.26f, unit * 0.5f).withTrimmedBottom (unit * 3.2f);
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

        g.setOpacity (1.0f);
        g.drawImage (background, getLocalBounds().toFloat());
        paintDropOverlay (g);
    }

    void ControllerView::renderBackground (float scale)
    {
        backgroundScale = scale;
        background = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                                  juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
        juce::Graphics g (background);
        g.addTransform (juce::AffineTransform::scale (scale));
        paintBackground (g);
    }

    void ControllerView::paintBackground (Graphics& g) const
    {
        // Transparente: el chasis amarillo lo pinta la ventana (MainComponent).
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
        const auto ink = textOnChassis;

        // --- Regla de TEMPO (serigrafía negra sobre el amarillo) --------------------------
        {
            const auto& slotR = L.tempoSlot;
            const auto capH = slotR.getWidth() * 0.6f * 0.38f;
            const auto top = slotR.getY() + capH * 0.5f + 1.0f, bottom = slotR.getBottom() - capH * 0.5f - 1.0f;
            const auto cx = slotR.getCentreX();
            const auto halfTrack = slotR.getWidth() * 0.6f * 0.5f;
            for (int i = 0; i <= 16; ++i)
            {
                const auto y = top + (bottom - top) * (float) i / 16.0f;
                const auto major = i % 4 == 0;
                const auto len = slotR.getWidth() * (major ? 0.36f : 0.22f);
                g.setColour (ink.withAlpha (major ? 0.95f : 0.7f));
                const auto thick = juce::jmax (1.0f, unit * (major ? 0.35f : 0.22f));
                g.fillRect (cx - halfTrack - unit * 0.6f - len, y - thick * 0.5f, len, thick);
                g.fillRect (cx + halfTrack + unit * 0.6f, y - thick * 0.5f, len, thick);
            }
            const auto signH = unit * 4.0f;
            const auto signX = L.mirrored ? slotR.getRight() - signH : slotR.getX() - unit * 0.6f;
            draw::text (g, "-", { signX, top - signH * 0.5f, signH, signH }, signH * 0.8f, ink);
            draw::text (g, "+", { signX, bottom - signH * 0.5f, signH, signH }, signH * 0.8f, ink);
            draw::text (g, "0", { signX, (top + bottom) * 0.5f - signH * 0.5f, signH, signH }, signH * 0.55f, ink);
            draw::text (g, tr ("deck.tempo"), L.tempoLabel, unit * 3.0f, ink);
        }

        // --- Plato: sombra y aro metálico (el vinilo que gira es PlatterView) ------------
        {
            const auto& p = L.platter;
            juce::Path disc;
            disc.addEllipse (p);
            if (const auto skin = SkinAssets::get().find ("platter", p.getWidth() * backgroundScale); skin.isValid())
            {
                g.setOpacity (1.0f);
                g.drawImage (skin, p.expanded (p.getWidth() * 0.04f), juce::RectanglePlacement::centred);
            }
            else
            {
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), juce::roundToInt (unit * 3.5f), { juce::roundToInt (unit * 0.6f), juce::roundToInt (unit * 1.4f) })
                .drawForPath (g, disc);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b4f57), p.getX(), p.getY(),
                                                     juce::Colour (0xff08090a), p.getRight(), p.getBottom(), false));
            g.fillPath (disc);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawEllipse (p.reduced (1.0f), juce::jmax (1.0f, unit * 0.3f));
            }

            // "PWD +" y "REV -" sobre arcos, abajo a cada lado del plato.
            const auto c = p.getCentre();
            const auto rr = p.getWidth() * 0.5f + unit * 2.6f;
            auto arcLabel = [&] (const juce::String& text, float angleDeg)
            {
                const auto a = juce::degreesToRadians (angleDeg);
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, rr - unit * 1.4f, rr - unit * 1.4f, 0.0f, a - 0.22f, a + 0.22f, true);
                g.setColour (ink);
                g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.0f, unit * 0.3f)));
                juce::GlyphArrangement ga;
                ga.addLineOfText (KbLookAndFeel::displayFont (unit * 2.6f, false), text, 0.0f, 0.0f);
                juce::Path tp;
                ga.createPath (tp);
                const auto tb = tp.getBounds();
                const auto pos = c.getPointOnCircumference (rr + unit * 0.6f, a);
                // Texto tangente al círculo (legible: rotado a lo largo del arco).
                const auto rot = a + (std::cos (a) < 0.0f ? juce::MathConstants<float>::pi : 0.0f) - juce::MathConstants<float>::halfPi;
                tp.applyTransform (juce::AffineTransform::translation (-tb.getCentreX(), -tb.getCentreY())
                                       .rotated (rot + juce::MathConstants<float>::pi * 0.5f)
                                       .translated (pos));
                g.fillPath (tp);
            };
            arcLabel ("PWD +", 220.0f);
            arcLabel ("REV -", 140.0f);
        }

        // --- Número del deck "(1)" -----------------------------------------------------------
        {
            juce::GlyphArrangement ga;
            ga.addFittedText (KbLookAndFeel::displayFont (L.number.getHeight() * 0.95f), "(" + juce::String (deckIndex + 1) + ")",
                              L.number.getX(), L.number.getY(), L.number.getWidth(), L.number.getHeight(),
                              L.mirrored ? Justification::centredLeft : Justification::centredRight, 1);
            juce::Path np;
            ga.createPath (np);
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.fillPath (np, juce::AffineTransform::translation (0.0f, 1.2f));
            g.setColour (ink);
            g.fillPath (np);
        }

        // "CUE" impreso debajo del botón de auriculares del canal.
        draw::text (g, tr ("mixer.cue"), L.chCueLabel, unit * 2.6f, ink);
    }

    void ControllerView::paintMixer (Graphics& g) const
    {
        const auto& M = mixer;

        juce::Path panelPath;
        panelPath.addRoundedRectangle (M.panel, unit * 2.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), juce::roundToInt (unit * 1.5f), {}).drawForPath (g, panelPath);
        if (const auto skin = SkinAssets::get().find ("mixer", M.panel.getWidth() * backgroundScale); skin.isValid())
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (panelPath);
            g.setOpacity (1.0f);
            g.drawImage (skin, M.panel, juce::RectanglePlacement::fillDestination);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (panelLight, 0.0f, M.panel.getY(), panelDark, 0.0f, M.panel.getBottom(), false));
            g.fillPath (panelPath);
        }
        g.setColour (panelEdge);
        g.strokePath (panelPath, juce::PathStrokeType (juce::jmax (1.0f, unit * 0.25f)));

        const auto textH = unit * 2.6f;
        draw::text (g, tr ("mixer.title"), M.title, unit * 3.6f, textPrimary);

        // GAIN, EQ, FILTER, CUE, PHONES, CUE MIX, SPLIT y vúmetros son componentes.
        // Etiquetas de los vúmetros de canal.
        draw::text (g, "1", M.meterL.withY (M.meterL.getBottom() + unit * 0.3f).withHeight (textH), textH * 0.8f, textSecondary);
        draw::text (g, "2", M.meterR.withY (M.meterR.getBottom() + unit * 0.3f).withHeight (textH), textH * 0.8f, textSecondary);
    }
    void ControllerView::mouseDown (const juce::MouseEvent& e) { nearestTouch.down (*this, e, unit * 4.5f); }
    void ControllerView::mouseDrag (const juce::MouseEvent& e) { nearestTouch.drag (e); }
    void ControllerView::mouseUp (const juce::MouseEvent& e)   { nearestTouch.up (e); }
} // namespace kb::ui
