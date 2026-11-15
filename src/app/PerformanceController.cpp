#include "app/PerformanceController.h"

#include "app/I18n.h"
#include "engine/AudioFileFormats.h"
#include "ui/Palette.h"
#include "ui/PlatterView.h"

#include <cmath>

namespace kb::app
{
    using engine::Deck;

    namespace
    {
        constexpr double kRollBeats[4] = { 0.125, 0.25, 0.5, 1.0 };
        constexpr double kLoopBeats[4] = { 1.0, 2.0, 4.0, 8.0 };
        const char* const kRollLabels[4] = { "1/8", "1/4", "1/2", "1" };
    }

    PerformanceController::PerformanceController (PlayerController& p, AppSettings& s)
        : player (p), engine (p.getEngine()), settings (s)
    {
        restoreSamplers();
    }

    PerformanceController::~PerformanceController() = default;

    juce::Colour PerformanceController::hotCueColour (int i)
    {
        return ui::palette::hotCueColour (i);
    }

    bool PerformanceController::isShift (int d) const
    {
        return shiftHeld[(size_t) d] || juce::ModifierKeys::currentModifiers.isShiftDown();
    }

    int PerformanceController::slotFor (int d, int padIndex) const
    {
        return d * kSlotsPerDeck + padIndex + (mode[(size_t) d] == PadMode::sampler2 ? 4 : 0);
    }

    // =========================================================================
    //  Botones de modo, SHIFT, SLIP, VINYL
    // =========================================================================
    void PerformanceController::modeButton (int d, int group)
    {
        static const PadMode primary[4]   = { PadMode::hotCue,  PadMode::fx,   PadMode::sampler,  PadMode::loop };
        static const PadMode secondary[4] = { PadMode::hotCue2, PadMode::roll, PadMode::sampler2, PadMode::loopManual };
        if (! juce::isPositiveAndBelow (d, 2) || ! juce::isPositiveAndBelow (group, 4))
            return;

        // Cambiar de modo suelta lo que estuviera apretado (roll, efectos).
        for (int i = 0; i < 4; ++i)
            pad (d, i, false);
        mode[(size_t) d] = isShift (d) ? secondary[group] : primary[group];
    }

    void PerformanceController::shift (int d, bool down)
    {
        if (juce::isPositiveAndBelow (d, 2))
            shiftHeld[(size_t) d] = down;
    }

    void PerformanceController::toggleSlip (int d)
    {
        slip[(size_t) d] = ! slip[(size_t) d];
        engine.setSlip (d, slip[(size_t) d]);
    }

    void PerformanceController::toggleVinyl (int d)
    {
        vinyl[(size_t) d] = ! vinyl[(size_t) d];
    }

    // =========================================================================
    //  Pads
    // =========================================================================
    void PerformanceController::pad (int d, int i, bool down)
    {
        if (! juce::isPositiveAndBelow (d, 2) || ! juce::isPositiveAndBelow (i, 4))
            return;

        switch (mode[(size_t) d])
        {
            case PadMode::hotCue:
            case PadMode::hotCue2:
            {
                if (! down)
                    return;
                const auto idx = i + (mode[(size_t) d] == PadMode::hotCue2 ? 4 : 0);
                if (isShift (d))
                    engine.setHotCue (d, idx, -1.0);   // SHIFT + pad: borrar
                else
                    engine.hotCue (d, idx);
                return;
            }

            case PadMode::fx:
            {
                if (i == 3)
                {
                    if (down)
                        engine.setReverse (d, ! engine.getDeckStatus (d).reverse);
                    return;
                }
                static const Deck::Effect fx[3] = { Deck::Effect::brake, Deck::Effect::backspin, Deck::Effect::censor };
                engine.setDeckEffect (d, fx[i], down);
                return;
            }

            case PadMode::roll:
                if (down)
                {
                    rollPad[(size_t) d] = i;
                    engine.loopRoll (d, kRollBeats[i], true);
                }
                else if (rollPad[(size_t) d] == i)
                {
                    rollPad[(size_t) d] = -1;
                    engine.loopRoll (d, kRollBeats[i], false);
                }
                return;

            case PadMode::sampler:
            case PadMode::sampler2:
            {
                if (! down)
                    return;
                const auto slot = slotFor (d, i);
                if (isShift (d))
                    engine.stopSample (slot);
                else if (sampleFiles[(size_t) slot] == juce::File())
                    chooseSample (slot);
                else
                    engine.triggerSample (slot);
                return;
            }

            case PadMode::loop:
                if (down)
                    engine.autoLoop (d, kLoopBeats[i]);
                return;

            case PadMode::loopManual:
                if (! down)
                    return;
                switch (i)
                {
                    case 0: engine.loopIn (d);     break;
                    case 1: engine.loopOut (d);    break;
                    case 2: engine.loopHalve (d);  break;
                    default: engine.loopDouble (d); break;
                }
                return;
        }
    }

    void PerformanceController::padMenu (int d, int i, juce::Component* target)
    {
        juce::PopupMenu menu;
        juce::WeakReference<PerformanceController> weak (this);
        const auto m = mode[(size_t) d];

        if (m == PadMode::hotCue || m == PadMode::hotCue2)
        {
            const auto idx = i + (m == PadMode::hotCue2 ? 4 : 0);
            menu.addItem (tr ("pad.deleteCue"), engine.getDeckStatus (d).hotCues[(size_t) idx] >= 0.0,
                          false, [weak, d, idx] { if (auto* s = weak.get()) s->engine.setHotCue (d, idx, -1.0); });
        }
        else if (m == PadMode::sampler || m == PadMode::sampler2)
        {
            const auto slot = slotFor (d, i);
            const bool loaded = sampleFiles[(size_t) slot] != juce::File();
            menu.addItem (tr ("pad.loadSample"), [weak, slot] { if (auto* s = weak.get()) s->chooseSample (slot); });
            menu.addSeparator();

            juce::PopupMenu volume;
            const auto current = engine.getSampleVolume (slot);
            for (auto v : { 1.0f, 0.8f, 0.6f, 0.4f, 0.2f })
                volume.addItem (juce::String (juce::roundToInt (v * 100.0f)) + " %", loaded, std::abs (current - v) < 0.01f,
                                [weak, slot, v] { if (auto* s = weak.get()) { s->engine.setSampleVolume (slot, v); s->saveSamplers(); } });
            menu.addSubMenu (tr ("pad.volume"), volume, loaded);

            const auto looping = engine.getSampleLooping (slot);
            menu.addItem (tr ("pad.oneShot"), loaded, ! looping,
                          [weak, slot] { if (auto* s = weak.get()) { s->engine.setSampleLooping (slot, false); s->saveSamplers(); } });
            menu.addItem (tr ("pad.loopMode"), loaded, looping,
                          [weak, slot] { if (auto* s = weak.get()) { s->engine.setSampleLooping (slot, true); s->saveSamplers(); } });
            menu.addSeparator();
            menu.addItem (tr ("pad.clearSample"), loaded, false, [weak, slot] { if (auto* s = weak.get()) s->clearSample (slot); });
        }
        else
        {
            return;
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target));
    }

    bool PerformanceController::dropOnPad (int d, int i, const juce::File& file)
    {
        const auto m = mode[(size_t) d];
        if (m != PadMode::sampler && m != PadMode::sampler2)
            return false;
        loadSample (slotFor (d, i), file);
        return true;
    }

    // =========================================================================
    //  Plato
    // =========================================================================
    void PerformanceController::jogTouch (int d, bool on)
    {
        touching[(size_t) d] = on;
        if (! vinyl[(size_t) d])
            return;
        if (on) engine.scratchBegin (d);
        else    engine.scratchEnd (d);
    }

    void PerformanceController::jogMove (int d, double rotations)
    {
        if (vinyl[(size_t) d] && touching[(size_t) d])
        {
            // Una vuelta del plato = una vuelta de un disco a 33⅓ rpm (1,8 s de audio).
            engine.scratchMove (d, rotations / ui::PlatterView::kRevolutionsPerSecond);
            return;
        }
        // Pitch bend: cuanto más rápido se gira, más se empuja (máx. ±30 %).
        engine.nudge (d, juce::jlimit (-0.3, 0.3, rotations * 4.0));
    }

    void PerformanceController::jogWheel (int d, float delta)
    {
        engine.nudge (d, juce::jlimit (-0.3, 0.3, (double) delta * 0.6));
    }

    // =========================================================================
    //  Sampler
    // =========================================================================
    void PerformanceController::chooseSample (int slot)
    {
        chooser = std::make_unique<juce::FileChooser> (tr ("pad.loadSample"),
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       engine::formats::fileWildcard());
        juce::WeakReference<PerformanceController> weak (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [weak, slot] (const juce::FileChooser& fc)
                              {
                                  if (auto* s = weak.get())
                                      if (fc.getResult() != juce::File())
                                          s->loadSample (slot, fc.getResult());
                              });
    }

    void PerformanceController::loadSample (int slot, const juce::File& file)
    {
        if (! juce::isPositiveAndBelow (slot, engine::Sampler::kNumSlots) || ! file.existsAsFile())
            return;

        const auto rate = engine.isPrepared() ? engine.getSampleRate() : 48000.0;
        juce::WeakReference<PerformanceController> weak (this);
        sampleLoader.loadAsync (slot, file, rate,
            [weak, slot, file] (std::shared_ptr<engine::TrackBuffer> t)
            {
                juce::MessageManager::callAsync ([weak, slot, file, t]
                {
                    if (auto* s = weak.get())
                    {
                        s->engine.loadSample (slot, t);
                        s->sampleFiles[(size_t) slot] = file;
                        s->saveSamplers();
                    }
                    else
                    {
                        t->cancelLoading();
                    }
                });
            },
            [file] (engine::LoadError)
            {
                juce::Logger::writeToLog ("Sampler: no se pudo cargar " + file.getFullPathName());
            });
    }

    void PerformanceController::clearSample (int slot)
    {
        engine.loadSample (slot, nullptr);
        sampleFiles[(size_t) slot] = juce::File();
        saveSamplers();
    }

    juce::String PerformanceController::getSampleName (int slot) const
    {
        return juce::isPositiveAndBelow (slot, engine::Sampler::kNumSlots)
                   ? sampleFiles[(size_t) slot].getFileNameWithoutExtension() : juce::String();
    }

    void PerformanceController::saveSamplers()
    {
        juce::Array<juce::var> list;
        for (int i = 0; i < engine::Sampler::kNumSlots; ++i)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("file", sampleFiles[(size_t) i].getFullPathName());
            o->setProperty ("volume", engine.getSampleVolume (i));
            o->setProperty ("loop", engine.getSampleLooping (i));
            list.add (juce::var (o));
        }
        settings.setSamplerState (juce::JSON::toString (juce::var (list), true));
    }

    void PerformanceController::restoreSamplers()
    {
        const auto json = juce::JSON::parse (settings.getSamplerState());
        if (const auto* list = json.getArray())
        {
            for (int i = 0; i < juce::jmin (engine::Sampler::kNumSlots, list->size()); ++i)
            {
                const auto& v = (*list)[i];
                engine.setSampleVolume (i, (float) (double) v.getProperty ("volume", 0.8));
                engine.setSampleLooping (i, (bool) v.getProperty ("loop", false));
                const juce::File f (v.getProperty ("file", {}).toString());
                if (v.getProperty ("file", {}).toString().isNotEmpty() && f.existsAsFile())
                    loadSample (i, f);
            }
        }
    }

    // =========================================================================
    //  Vista
    // =========================================================================
    ui::ControllerView::PadView PerformanceController::makePadView (int d) const
    {
        using namespace ui::palette;
        ui::ControllerView::PadView v;
        const auto st = engine.getDeckStatus (d);
        const auto m = mode[(size_t) d];
        v.shift = isShift (d);
        v.slip  = slip[(size_t) d];
        v.vinyl = vinyl[(size_t) d];

        switch (m)
        {
            case PadMode::hotCue:   case PadMode::hotCue2:  v.group = 0; break;
            case PadMode::fx:       case PadMode::roll:     v.group = 1; break;
            case PadMode::sampler:  case PadMode::sampler2: v.group = 2; break;
            case PadMode::loop:     case PadMode::loopManual: v.group = 3; break;
        }
        v.secondary = (m == PadMode::hotCue2 || m == PadMode::roll || m == PadMode::sampler2 || m == PadMode::loopManual);

        const auto track = player.getTrack (d);
        const auto bpm = track != nullptr && track->getBpm() > 0.0 ? track->getBpm() : 120.0;
        const auto loopBeats = st.loopActive ? (st.loopOutSeconds - st.loopInSeconds) * bpm / 60.0 : -1.0;

        for (int i = 0; i < 4; ++i)
        {
            auto& label = v.labels[(size_t) i];
            auto& colour = v.colours[(size_t) i];
            auto& lit = v.lit[(size_t) i];
            switch (m)
            {
                case PadMode::hotCue:
                case PadMode::hotCue2:
                {
                    const auto idx = i + (m == PadMode::hotCue2 ? 4 : 0);
                    label  = juce::String (idx + 1);
                    colour = hotCueColour (idx);
                    lit    = st.hotCues[(size_t) idx] >= 0.0;
                    break;
                }
                case PadMode::fx:
                {
                    static const char* keys[4] = { "pad.brake", "pad.backspin", "pad.censor", "pad.reverse" };
                    label  = tr (keys[i]);
                    colour = alert;
                    lit    = (i == 0 && st.effect == (int) Deck::Effect::brake)
                          || (i == 1 && st.effect == (int) Deck::Effect::backspin)
                          || (i == 2 && st.effect == (int) Deck::Effect::censor)
                          || (i == 3 && st.reverse);
                    break;
                }
                case PadMode::roll:
                    label  = kRollLabels[i];
                    colour = juce::Colour (0xffA85CFF);
                    lit    = st.rolling && rollPad[(size_t) d] == i;
                    break;
                case PadMode::sampler:
                case PadMode::sampler2:
                {
                    const auto slot = slotFor (d, i);
                    const auto name = getSampleName (slot);
                    // "Artista - Título" -> se muestra el título (lo que mejor identifica al sample).
                    const auto shortName = name.containsIgnoreCase (" - ") ? name.fromLastOccurrenceOf (" - ", false, false) : name;
                    label  = name.isEmpty() ? juce::String ("+") : shortName.substring (0, 12).trim().toUpperCase();
                    colour = name.isEmpty() ? cyan.withAlpha (0.55f) : cyan;
                    v.progress[(size_t) i] = engine.getSampleProgress (slot);
                    lit    = v.progress[(size_t) i] >= 0.0f;
                    break;
                }
                case PadMode::loop:
                    label  = juce::String ((int) kLoopBeats[i]);
                    colour = play;
                    lit    = st.loopActive && ! st.rolling && std::abs (loopBeats - kLoopBeats[i]) < 0.02;
                    break;
                case PadMode::loopManual:
                {
                    static const char* keys[4] = { "pad.loopIn", "pad.loopOut", "pad.loopHalf", "pad.loopDouble" };
                    label  = tr (i == 1 && st.loopActive ? "pad.loopExit" : keys[i]);
                    colour = play;
                    lit    = (i == 1 && st.loopActive) || (i == 0 && st.loopInSeconds >= 0.0 && ! st.loopActive && st.loopOutSeconds < 0.0);
                    break;
                }
            }
        }
        return v;
    }
} // namespace kb::app
