#include "app/MainComponent.h"

#include <map>

#include "app/AudioDeviceService.h"
#include "app/PlatformFiles.h"
#include "engine/AudioFileFormats.h"
#include "analysis/KeyDetector.h"
#include "ui/KbLookAndFeel.h"
#include "ui/Palette.h"
#include "ui/SkinAssets.h"

namespace kb::app
{
    static constexpr int kUiRefreshHz = 30;

    MainComponent::MainComponent (AudioDeviceService& a, PlayerController& p, TrackLibrary& lib, AppSettings& s)
        : audio (a), settings (s), player (p), library (lib), libraryView (lib)
    {
        setOpaque (true);
        for (auto* c : std::initializer_list<juce::Component*> { &topBar, &controller, &deckDisplays[0],
                                                                 &deckDisplays[1], &libraryView, &statusBar, &fxPanel })
            addAndMakeVisible (c);

        // Visor del controlador para teléfonos (Fase 10): sin barras ni arrastre (los
        // gestos son de los platos y faders); se cambia de deck con el botón lateral.
       #if JUCE_ANDROID || JUCE_IOS
        longPressEnabled = true;
       #else
        longPressEnabled = juce::SystemStats::getEnvironmentVariable ("KB_DEV_LONGPRESS", {}).isNotEmpty();
       #endif
        controllerHolder.setScrollBarsShown (false, false, false, false);
        controllerHolder.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
        deckSwitch.setComponentID ("deckSwitch");
        deckSwitch.onClick = [this]
        {
            compactPage = 1 - compactPage;
            resized();
        };

        libraryView.setArtworkCache (&artwork);
        libraryView.restoreColumns (settings.getLibraryColumns());
        libraryView.onColumnsChanged = [this] (const juce::String& st) { settings.setLibraryColumns (st); };

        libraryView.onLoadRequest = [this] (int deck, const juce::File& file)
        {
            player.loadFile (deck >= 0 ? deck : chooseFreeDeck(), file);
        };

        // --- Controlador -> motor --------------------------------------------------------
        auto& engine = player.getEngine();
        controller.onPlay       = [this] (int d) { player.play (d); };
        controller.onPause      = [this] (int d) { player.pause (d); };
        controller.onCueDown    = [this] (int d) { player.cueDown (d); };
        controller.onCueUp      = [this] (int d) { player.cueUp (d); };
        controller.onVolume     = [&engine] (int d, float v) { engine.setChannelVolume (d, v); };
        controller.onCrossfader = [&engine] (float v) { engine.setCrossfader (v); };
        controller.onMaster     = [&engine] (float v) { engine.setMasterVolume (v); };
        controller.onTrim       = [&engine] (int d, float v) { engine.setTrim (d, v); };
        controller.onEq         = [&engine] (int d, int band, float v) { engine.setEq (d, (engine::EqBand) band, v); };
        controller.onFilter     = [&engine] (int d, float v) { engine.setFilter (d, v); };
        controller.onChannelCue = [&engine] (int d, bool on) { engine.setChannelCue (d, on); };
        controller.onPhones     = [&engine] (float v) { engine.setHeadphoneVolume (v); };
        controller.onCueMix     = [&engine] (float v) { engine.setCueMix (v); };
        controller.onSplit      = [&engine] (bool on) { engine.setSplit (on); };
        controller.onTempo      = [this] (int d, float pct) { player.setTempo (d, pct); };
        controller.onSync       = [this] (int d) { player.toggleSync (d); };
        controller.onKeyLock    = [&engine] (int d) { engine.setKeyLock (d, ! engine.getKeyLock (d)); };
        controller.onKeyShift   = [&engine] (int d, int delta)
        {
            engine.setKeyShift (d, delta == 0 ? 0 : engine.getKeyShift (d) + delta);
        };

        // --- Performance: pads, plato, SHIFT / SLIP / VINYL (Fase 4) -------------------------
        performance = std::make_unique<PerformanceController> (player, settings);
        auto* perf = performance.get();
        controller.onPadMode  = [perf] (int d, int g) { perf->modeButton (d, g); };
        controller.onPad      = [perf] (int d, int i, bool down) { perf->pad (d, i, down); };
        controller.onPadMenu  = [this, perf] (int d, int i, juce::Component* t)
        {
            const auto target = "deck" + juce::String (d + 1) + ".pad" + juce::String (i + 1);
            perf->padMenu (d, i, t, [this, target] (juce::PopupMenu& m) { addMidiItems (m, target); });
        };
        controller.onShift    = [perf] (int d, bool down) { perf->shift (d, down); };
        controller.onSlip     = [perf] (int d) { perf->toggleSlip (d); };
        controller.onVinyl    = [perf] (int d) { perf->toggleVinyl (d); };
        controller.onJogTouch = [perf] (int d, bool t) { perf->jogTouch (d, t); };
        controller.onJogTurn  = [perf] (int d, double turns) { perf->jogMove (d, turns); };
        controller.onJogWheel = [perf] (int d, float w) { perf->jogWheel (d, w); };

        // --- GPU (OpenGL), según Preferencias ---------------------------------------------
        updateRenderer();

       #if JUCE_ANDROID
        // Primera vez en Android: pedir permiso y traer la música del dispositivo.
        if (library.collectionSize() == 0)
        {
            juce::Component::SafePointer<MainComponent> safe (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->libraryView.scanDeviceMusic(); });
        }
       #endif

        // --- MIDI (Fase 8) ---------------------------------------------------------------------
        midiController = std::make_unique<MidiController> (AppSettings::getAppDataDirectory().getChildFile ("midi_mapping.json"));
        midiController->onEvent  = [this] (const midi::Event& e) { handleMidi (e); };
        midiController->ledState = [this] (const juce::String& t) { return midiLedState (t); };
        midiController->onLearned = [this] (const juce::String& t, const midi::Binding& b)
        {
            midiStatus = tr ("midi.learned", { { "name", midiTargetName (t) }, { "msg", b.describe() } });
            midiStatusTicks = kUiRefreshHz * 4;
            updateStatusBar();
        };
        midiController->onMappingChanged = [this] { updateStatusBar(); };
        // Atajos de teclado (Fase 9).
        if (settings.getKeymap().isNotEmpty())
            keymap.fromJson (settings.getKeymap());
        // Click derecho sobre cualquier control -> MIDI Learn (ver mouseDown).
        addMouseListener (this, true);

        // --- Vista "Biblioteca grande" (Fase 7) --------------------------------------------
        largeLibrary = settings.getLargeLibrary();
        libraryMix = largeLibrary ? 1.0f : 0.0f;
        topBar.setLibraryViewLit (largeLibrary);
        topBar.onLibraryView = [this] { setLargeLibrary (! largeLibrary); };
        setWantsKeyboardFocus (true);

        // --- Efectos (Fase 5) ----------------------------------------------------------------
        restoreFx();
        fxPanel.onType   = [this] (int u, int t)    { player.getEngine().getFxUnit (u).setType ((engine::fx::Type) t); saveFx(); updateFxPanel(); };
        fxPanel.onOn     = [this] (int u, bool on)  { player.getEngine().getFxUnit (u).setOn (on); updateFxPanel(); };
        fxPanel.onWet    = [this] (int u, float w)  { player.getEngine().getFxUnit (u).setWet (w); saveFx(); };
        fxPanel.onParam  = [this] (int u, float v)  { player.getEngine().getFxUnit (u).setParam (v); saveFx(); };
        fxPanel.onBeats  = [this] (int u, int i)    { player.getEngine().getFxUnit (u).setBeatsIndex (i); saveFx(); updateFxPanel(); };
        fxPanel.onTarget = [this] (int u, int t)    { player.getEngine().getFxUnit (u).setTarget ((engine::fx::Target) t); saveFx(); updateFxPanel(); };
        updateFxPanel();

        // --- AUTO MIX ------------------------------------------------------------------------
        autoMixSeconds = settings.getAutoMixSeconds();
        controller.setAutoMixSeconds (autoMixSeconds);
        controller.onAutoMix     = [this] { toggleAutoMix(); };
        controller.onAutoMixTime = [this] (double secs)
        {
            autoMixSeconds = secs;
            settings.setAutoMixSeconds (secs);
        };

        for (int d = 0; d < 2; ++d)
        {
            controller.setVolume (d, engine.getChannelVolume (d));
            controller.setTrim (d, engine.getTrim (d));
            controller.setFilter (d, engine.getFilter (d));
            for (int band = 0; band < 3; ++band)
                controller.setEq (d, band, engine.getEq (d, (engine::EqBand) band));
            controller.setChannelCue (d, engine.getChannelCue (d));
            deckDisplays[d].onSeek = [this, d] (double seconds) { player.seek (d, seconds); };
        }
        controller.setCrossfader (engine.getCrossfader());
        controller.setMaster (engine.getMasterVolume());
        controller.setPhones (engine.getHeadphoneVolume());
        controller.setCueMix (engine.getCueMix());
        controller.setSplit (engine.getSplit());

        // Vúmetros a 60 fps sincronizados con el refresco de la pantalla.
        vblank = std::make_unique<juce::VBlankAttachment> (this, [this] { updateMeters(); });

        I18n::getInstance().addListener (this);
        player.addListener (this);
        updateStatusBar();
        startTimerHz (kUiRefreshHz);
    }

    MainComponent::~MainComponent()
    {
        if (openGLAttached)
            openGL.detach();
        player.removeListener (this);
        I18n::getInstance().removeListener (this);
    }

    int MainComponent::frameThickness() const
    {
        return juce::jmax (10, juce::roundToInt (getHeight() * 0.016));
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (frameImage.isNull() || ! juce::exactlyEqual (scale, frameScale)
            || frameImage.getWidth() != juce::roundToInt ((float) getWidth() * scale))
            renderFrame (scale);
        g.drawImage (frameImage, getLocalBounds().toFloat());
    }

    void MainComponent::renderFrame (float scale)
    {
        // Chasis amarillo liso con volumen (como la imagen de referencia) y la
        // "pantalla" oscura empotrada abajo (waveforms + biblioteca).
        using namespace ui::palette;
        frameScale = scale;
        frameImage = juce::Image (juce::Image::RGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                                  juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
        juce::Graphics g (frameImage);
        g.addTransform (juce::AffineTransform::scale (scale));

        const auto b = getLocalBounds().toFloat();
        const auto m = (float) frameThickness();
        const auto r = m * 2.4f;

        g.fillAll (juce::Colour (0xff0b0c0e));

        juce::Path body;
        body.addRoundedRectangle (b.reduced (1.0f), r);
        // Render del chasis (skin/chassis.png) si existe; si no, por código.
        if (const auto skin = ui::SkinAssets::get().find ("chassis", b.getWidth() * scale); skin.isValid())
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (body);
            g.setOpacity (1.0f);
            g.drawImage (skin, b, juce::RectanglePlacement::fillDestination);
        }
        else
        {
            juce::ColourGradient grad (chassisLight, 0.0f, 0.0f, chassisShadow, 0.0f, b.getBottom(), false);
            grad.addColour (0.18, chassis);
            grad.addColour (0.8, chassis.darker (0.08f));
            g.setGradientFill (grad);
            g.fillPath (body);
        }
        // Brillo de borde (plástico) y contorno.
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (2.5f), r - 1.5f, 1.6f);
        g.setColour (chassisShadow.darker (0.7f));
        g.drawRoundedRectangle (b.reduced (1.0f), r, 1.2f);

        // Pantalla empotrada: sombra arriba (hundida) y brillo abajo.
        const auto screen = screenArea.toFloat();
        const auto sr = m * 1.4f;
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRoundedRectangle (screen.translated (0.0f, 1.5f).expanded (1.0f), sr);
        g.setColour (chassisShadow.darker (0.9f));
        g.fillRoundedRectangle (screen.expanded (2.0f), sr + 1.0f);
        g.setColour (juce::Colour (0xff0e1013));
        g.fillRoundedRectangle (screen, sr);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, screen.getY(),
                                                 juce::Colours::transparentBlack, 0.0f, screen.getY() + m, false));
        g.fillRoundedRectangle (screen.withHeight (m), sr);
    }

    juce::Rectangle<int> MainComponent::controllerArea() const
    {
        // El controlador puede estar dentro del visor de teléfono (Fase 10).
        const auto* holder = compact ? static_cast<const juce::Component*> (&controllerHolder) : &controller;
        return holder->isVisible() ? holder->getBounds() : juce::Rectangle<int>();
    }

    void MainComponent::resized()
    {
        frameImage = {};
        repaint();   // el chasis y la pantalla negra cambian de forma (p. ej. al animar la vista)

        // Teléfono (Fase 10): pantalla baja (menos de 560 puntos de alto) -> vista compacta.
        const auto wantCompact = getHeight() < kCompactMaxHeight;
        if (wantCompact != compact)
        {
            compact = wantCompact;
            if (compact)
            {
                controllerHolder.setViewedComponent (&controller, false);
                addAndMakeVisible (controllerHolder);
                addAndMakeVisible (deckSwitch);
            }
            else
            {
                controllerHolder.setViewedComponent (nullptr, false);
                removeChildComponent (&controllerHolder);
                removeChildComponent (&deckSwitch);
                addAndMakeVisible (controller);
            }
            statusBar.setVisible (! compact);
        }

        if (compact)
        {
            layoutCompact();
            return;
        }

        // Layout de la imagen de referencia: barra superior amarilla, controlador,
        // pantalla oscura con las dos waveforms lado a lado y la biblioteca, y pie.
        const auto H = getHeight();
        // Escala de la interfaz para pantallas grandes (4K al 100 %): base 1040 px de alto.
        ui::KbLookAndFeel::setUiScale ((float) H / 1040.0f);
        const auto m = frameThickness();
        auto b = getLocalBounds().reduced (m + m / 2, m / 2);

        statusBar.setBounds (b.removeFromBottom (juce::jmax (20, juce::roundToInt (H * 0.034))));
        // Biblioteca grande: el controlador pasa de 46,5 % a 26 % del alto (animado).
        const auto mix = (double) libraryMix;
        topBar.setBounds     (b.removeFromTop (juce::roundToInt (H * 0.075)));
        controller.setBounds (b.removeFromTop (juce::roundToInt (H * (0.465 - 0.205 * mix))));
        b.removeFromTop (m / 2);
        b.removeFromBottom (m / 3);
        screenArea = b;

        auto inner = b.reduced (m / 2 + 2);
        auto waves = inner.removeFromTop (juce::roundToInt (inner.getHeight() * (0.56 - 0.34 * mix)));
        const auto gap = m / 2;
        deckDisplays[0].setBounds (waves.removeFromLeft ((waves.getWidth() - gap) / 2));
        waves.removeFromLeft (gap);
        deckDisplays[1].setBounds (waves);
        inner.removeFromTop (m / 3);
        fxPanel.setBounds (inner.removeFromTop (juce::jmax (34, juce::roundToInt (H * 0.045))));
        inner.removeFromTop (m / 3);
        libraryView.setBounds (inner);
    }

    void MainComponent::layoutCompact()
    {
        // Teléfono en horizontal: el controlador completo no entra a un tamaño tocable,
        // así que se muestra a su proporción natural dentro de un visor y se ve
        // "DECK 1 + MIXER" o "MIXER + DECK 2" (botón lateral para cambiar).
        // BIBLIOTECA cambia a la vista de biblioteca (waveforms chicas + FX + lista).
        const auto H = getHeight();
        const auto W = getWidth();
        ui::KbLookAndFeel::setUiScale ((float) H / 600.0f);   // textos más grandes que en escritorio
        const auto m = juce::jmax (6, juce::roundToInt (H * 0.014));
        auto b = getLocalBounds().reduced (m, m / 2);

        const auto mix = (double) libraryMix;
        topBar.setBounds (b.removeFromTop (juce::roundToInt (H * 0.12)));

        const auto ctrlH = juce::roundToInt (H * 0.62 * (1.0 - mix));
        controllerHolder.setVisible (ctrlH > 8);
        deckSwitch.setVisible (ctrlH > 8);
        if (ctrlH > 8)
        {
            auto row = b.removeFromTop (ctrlH);
            // Cada página muestra un deck + el mixer (~63 % del controlador) a todo el ancho.
            const auto naturalW = juce::jmax (juce::roundToInt (ctrlH * kControllerAspect),
                                              juce::roundToInt ((row.getWidth() - H * 0.085) / 0.635));
            const auto tabW = juce::roundToInt (H * 0.085);
            const auto fits = naturalW <= row.getWidth();
            deckSwitch.setVisible (! fits);
            if (! fits)
            {
                // Botón del lado hacia donde se va: a la derecha "2 ›", a la izquierda "‹ 1".
                deckSwitch.setBounds (compactPage == 0 ? row.removeFromRight (tabW) : row.removeFromLeft (tabW));
                deckSwitch.setButtonText (compactPage == 0 ? juce::String::fromUTF8 ("2 \xe2\x96\xb6") : juce::String::fromUTF8 ("\xe2\x97\x80 1"));
            }
            controllerHolder.setBounds (row);
            controller.setSize (fits ? row.getWidth() : naturalW, row.getHeight());
            const auto maxX = juce::jmax (0, controller.getWidth() - row.getWidth());
            controllerHolder.setViewPosition (compactPage == 0 ? 0 : maxX, 0);
        }
        b.removeFromTop (m / 2);
        screenArea = b;

        auto inner = b.reduced (m / 2 + 1);
        const auto gap = m / 2;
        auto waves = inner.removeFromTop (juce::roundToInt (inner.getHeight() * (1.0 - 0.72 * mix)));
        deckDisplays[0].setBounds (waves.removeFromLeft ((waves.getWidth() - gap) / 2));
        waves.removeFromLeft (gap);
        deckDisplays[1].setBounds (waves);

        const auto showLibrary = mix > 0.05;
        fxPanel.setVisible (showLibrary);
        libraryView.setVisible (showLibrary);
        if (showLibrary)
        {
            inner.removeFromTop (m / 3);
            fxPanel.setBounds (inner.removeFromTop (juce::jmax (30, juce::roundToInt (H * 0.08))));
            inner.removeFromTop (m / 3);
            libraryView.setBounds (inner);
        }
        juce::ignoreUnused (W);
    }

    // =========================================================================
    //  Carga de archivos
    // =========================================================================
    void MainComponent::openLoadDialog (int deck)
    {
        chooser = std::make_unique<juce::FileChooser> (tr ("load.chooserTitle", { { "n", juce::String (deck + 1) } }),
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       engine::formats::fileWildcard());

        juce::Component::SafePointer<MainComponent> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe, deck] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr)
                                      return;
                                  platform::resolve (fc.getURLResults(), [safe, deck] (juce::Array<juce::File> files)
                                  {
                                      if (safe != nullptr && ! files.isEmpty())
                                      {
                                          safe->library.addFiles (files);   // también queda en la biblioteca
                                          safe->player.loadFile (deck, files.getFirst());
                                      }
                                  });
                              });
    }

    void MainComponent::trackLoaded (int deck)
    {
        playedMarked[(size_t) deck] = false;
        loadDeckMetadata (deck);
        deckDisplays[deck].update (makeDeckView (deck));
    }

    void MainComponent::loadDeckMetadata (int deck)
    {
        const auto track = player.getTrack (deck);
        const auto file = track != nullptr ? track->getInfo().file : juce::File();
        deckFile[(size_t) deck] = file.getFullPathName();
        deckArtwork[(size_t) deck] = {};
        deckTags[(size_t) deck] = {};
        controller.setPlatterArtwork (deck, {});
        if (file == juce::File())
            return;

        // Carátula (en segundo plano): en el plato, girando, y en el display del deck.
        juce::Component::SafePointer<MainComponent> safe (this);
        const auto path = file.getFullPathName();
        artwork.request (file, [safe, deck, path] (const juce::Image& img)
        {
            if (safe == nullptr || safe->deckFile[(size_t) deck] != path)
                return;   // ya se cargó otra canción en ese deck
            safe->deckArtwork[(size_t) deck] = img;
            safe->controller.setPlatterArtwork (deck, img);
            safe->deckDisplays[deck].update (safe->makeDeckView (deck));
        });

        // Título y artista de los tags (en segundo plano: el archivo puede estar en red).
        juce::Thread::launch ([safe, deck, file, path]
        {
            auto tags = library::TagReader::read (file);
            juce::MessageManager::callAsync ([safe, deck, path, tags]
            {
                if (safe == nullptr || safe->deckFile[(size_t) deck] != path)
                    return;
                safe->deckTags[(size_t) deck] = tags;
                safe->deckDisplays[deck].update (safe->makeDeckView (deck));
            });
        });
    }

    void MainComponent::trackLoadFailed (int, const juce::File& file, engine::LoadError error)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::WarningIcon)
                                          .withTitle (tr ("load.error.title"))
                                          .withMessage (tr ("load.error.body", { { "file", file.getFileName() },
                                                                                  { "reason", describeError (error) } }))
                                          .withButton (tr ("common.ok"))
                                          .withAssociatedComponent (this),
                                      nullptr);
    }

    void MainComponent::updateRenderer()
    {
        const auto want = settings.getUseOpenGL();
        if (want == openGLAttached)
            return;
        if (want)
        {
            // La interfaz entera se pinta en la GPU; si el driver no da OpenGL, JUCE
            // sigue con el dibujo por software (no se cae).
            openGL.setComponentPaintingEnabled (true);
            openGL.setContinuousRepainting (false);
            openGL.attachTo (*this);
        }
        else
        {
            openGL.detach();
        }
        openGLAttached = want;
        juce::Logger::writeToLog (juce::String ("Dibujo: ") + (want ? "GPU (OpenGL)" : "software"));
    }

    void MainComponent::setLargeLibrary (bool on)
    {
        largeLibrary = on;
        settings.setLargeLibrary (on);
        topBar.setLibraryViewLit (on);
    }

    bool MainComponent::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey && midiController->isLearning())
        {
            midiController->cancelLearn();
            return true;
        }
        // Atajos configurables: la tecla aprieta el "botón"; al soltarla, lo suelta.
        const auto target = keymap.targetFor (key);
        if (target.isEmpty())
            return false;
        const auto code = key.getKeyCode();
        for (const auto& h : heldKeys)
            if (h.first == code)
                return true;   // repetición automática del teclado: ignorar
        heldKeys.emplace_back (code, target);
        midi::Event e;
        e.target = target;
        e.kind = midi::TargetKind::button;
        e.pressed = true;
        handleMidi (e);
        return true;
    }

    bool MainComponent::keyStateChanged (bool)
    {
        bool handled = false;
        for (auto it = heldKeys.begin(); it != heldKeys.end();)
        {
            if (juce::KeyPress::isKeyCurrentlyDown (it->first))
            {
                ++it;
                continue;
            }
            midi::Event e;
            e.target = it->second;
            e.kind = midi::TargetKind::button;
            e.pressed = false;
            handleMidi (e);
            it = heldKeys.erase (it);
            handled = true;
        }
        return handled;
    }

    // =========================================================================
    //  MIDI (Fase 8)
    // =========================================================================
    juce::String MainComponent::midiTargetName (const juce::String& id)
    {
        static const std::map<juce::String, juce::String> names {
            { "play", "PLAY" }, { "pause", "PAUSE" }, { "playPause", "PLAY/PAUSE" }, { "cue", "CUE" }, { "sync", "SYNC" },
            { "keyLock", "KEY LOCK" }, { "keyDown", "KEY -" }, { "keyUp", "KEY +" }, { "slip", "SLIP" }, { "vinyl", "VINYL" },
            { "shift", "SHIFT" }, { "modeHotCue", "HOT CUE" }, { "modeFx", "FX" }, { "modeSampler", "SAMPLER" }, { "modeLoop", "LOOP" },
            { "loopIn", "LOOP IN" }, { "loopOut", "LOOP OUT" }, { "loopHalve", "LOOP 1/2" }, { "loopDouble", "LOOP x2" },
            { "pad1", "PAD 1" }, { "pad2", "PAD 2" }, { "pad3", "PAD 3" }, { "pad4", "PAD 4" }, { "jogTouch", "JOG TOUCH" },
            { "jog", "JOG" }, { "load", "LOAD" }, { "tempo", "TEMPO" }, { "tempoRange", "TEMPO RANGE" }, { "tempoNudge", "PITCH BEND" },
            { "volume", "VOLUME" }, { "gain", "GAIN" }, { "high", "HIGH" }, { "mid", "MID" }, { "low", "LOW" }, { "filter", "FILTER" },
            { "crossfader", "CROSSFADER" }, { "master", "MASTER" }, { "phones", "PHONES" }, { "cueMix", "CUE MIX" },
            { "mixTime", "MIX TIME" }, { "split", "SPLIT" }, { "autoMix", "AUTO MIX" }, { "on", "ON" }, { "wet", "WET" },
            { "param", "PARAM" }, { "beatsDown", "BEAT <" }, { "beatsUp", "BEAT >" }, { "browse", "BROWSE" }, { "largeView", "BIBLIOTECA" } };
        const auto parts = juce::StringArray::fromTokens (id, ".", "");
        const auto last = parts[parts.size() - 1];
        const auto it = names.find (last);
        const auto func = it != names.end() ? it->second : last.toUpperCase();
        const auto first = parts[0];
        if (first.startsWith ("deck"))
            return tr ("midi.deck", { { "n", first.getLastCharacters (1) } }) + " · " + func;
        if (first.startsWith ("fx"))
            return "FX " + first.getLastCharacters (1) + " · " + func;
        if (first == "mixer")
            return parts.size() == 3 ? tr ("midi.channel", { { "n", parts[1].getLastCharacters (1) } }) + " · " + func
                                     : tr ("midi.mixer") + " · " + func;
        return tr ("midi.library") + " · " + func;
    }

    void MainComponent::addMidiItems (juce::PopupMenu& menu, const juce::String& target)
    {
        if (! midi::findTarget (target).has_value())
            return;
        juce::Component::SafePointer<MainComponent> safe (this);
        menu.addItem (tr ("midi.learnFor", { { "name", midiTargetName (target) } }), [safe, target]
        {
            if (safe == nullptr) return;
            safe->midiController->startLearn (target);
            safe->updateStatusBar();
        });
        for (const auto& b : midiController->getMap().bindingsFor (target))
        {
            menu.addItem (tr ("midi.forget", { { "msg", b.describe() } }), [safe, target]
            {
                if (safe != nullptr) safe->midiController->removeTarget (target);
            });
            break;
        }
    }

    void MainComponent::showMidiLearnMenu (const juce::String& target, juce::Component* near)
    {
        juce::PopupMenu menu;
        addMidiItems (menu, target);
        if (menu.getNumItems() > 0)
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (near));
    }

    void MainComponent::mouseDown (const juce::MouseEvent& e)
    {
        // Solo botones/pads y la biblioteca: en platos, faders y perillas dejar el dedo
        // quieto es parte de tocar (frenar el disco), no un pedido de menú.
        const auto longPressTarget = e.eventComponent != nullptr
                                  && (dynamic_cast<juce::Button*> (e.eventComponent) != nullptr
                                      || libraryView.isParentOf (e.eventComponent));
        if (longPressEnabled && longPressTarget && ! e.mods.isPopupMenu() && e.eventComponent != this)
        {
            longPress.target = e.eventComponent;
            longPress.source = e.source;
            longPress.position = e.position;
            longPress.startMs = juce::Time::getMillisecondCounterHiRes();
            longPress.armed = true;
        }
        if (! e.mods.isPopupMenu() || e.eventComponent == nullptr)
            return;
        // El control (o un padre) con un id de función MIDI. Los pads tienen su propio menú.
        for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
        {
            const auto id = c->getComponentID();
            if (id.isEmpty())
                continue;
            if (id.contains (".pad"))
                return;
            if (midi::findTarget (id).has_value())
            {
                showMidiLearnMenu (id, c);
                return;
            }
        }
    }

    void MainComponent::mouseDrag (const juce::MouseEvent& e)
    {
        // Si el dedo se mueve no es "mantener apretado" (es girar un plato, mover un fader...).
        if (longPress.armed && e.getDistanceFromDragStart() > 12)
            longPress.armed = false;
    }

    void MainComponent::mouseUp (const juce::MouseEvent&)
    {
        longPress.armed = false;
    }

    void MainComponent::checkLongPress()
    {
        if (! longPress.armed || juce::Time::getMillisecondCounterHiRes() - longPress.startMs < 550.0)
            return;
        longPress.armed = false;
        auto* target = longPress.target.getComponent();
        if (target == nullptr || ! target->isShowing())
            return;

        // Se simula un click derecho sobre el mismo control (y se avisa a la ventana para MIDI Learn).
        const auto now = juce::Time::getCurrentTime();
        const juce::MouseEvent right (longPress.source, longPress.position,
                                      juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier),
                                      juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                                      juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
                                      juce::MouseInputSource::defaultTiltY, target, target, now,
                                      longPress.position, now, 1, false);
        target->mouseDown (right);
        // Las filas de la tabla abren su menú al soltar (la fila ya estaba elegida).
        if (libraryView.isParentOf (target))
            target->mouseUp (right);
        mouseDown (right);
    }

    void MainComponent::handleMidi (const midi::Event& e)
    {
        auto& engine = player.getEngine();
        auto* perf = performance.get();
        const auto parts = juce::StringArray::fromTokens (e.target, ".", "");
        const auto& head = parts[0];
        const auto& func = parts[parts.size() - 1];
        const bool press = e.kind == midi::TargetKind::button && e.pressed;

        if (head.startsWith ("deck"))
        {
            const auto d = head.getLastCharacters (1).getIntValue() - 1;
            if (! juce::isPositiveAndBelow (d, 2)) return;
            if (func == "play")       { if (press) player.play (d); }
            else if (func == "pause") { if (press) player.pause (d); }
            else if (func == "playPause") { if (press) player.togglePlay (d); }
            else if (func == "cue")   { if (press) player.cueDown (d); else player.cueUp (d); }
            else if (func == "sync")  { if (press) player.toggleSync (d); }
            else if (func == "keyLock") { if (press) engine.setKeyLock (d, ! engine.getKeyLock (d)); }
            else if (func == "keyDown") { if (press) engine.setKeyShift (d, engine.getKeyShift (d) - 1); }
            else if (func == "keyUp")   { if (press) engine.setKeyShift (d, engine.getKeyShift (d) + 1); }
            else if (func == "slip")  { if (press) perf->toggleSlip (d); }
            else if (func == "vinyl") { if (press) perf->toggleVinyl (d); }
            else if (func == "shift") { perf->shift (d, e.pressed); }
            else if (func == "modeHotCue")  { if (press) perf->modeButton (d, 0); }
            else if (func == "modeFx")      { if (press) perf->modeButton (d, 1); }
            else if (func == "modeSampler") { if (press) perf->modeButton (d, 2); }
            else if (func == "modeLoop")    { if (press) perf->modeButton (d, 3); }
            else if (func == "loopIn")     { if (press) engine.loopIn (d); }
            else if (func == "loopOut")    { if (press) engine.loopOut (d); }
            else if (func == "loopHalve")  { if (press) engine.loopHalve (d); }
            else if (func == "loopDouble") { if (press) engine.loopDouble (d); }
            else if (func.startsWith ("pad")) { perf->pad (d, func.getLastCharacters (1).getIntValue() - 1, e.pressed); }
            else if (func == "jogTouch") { perf->jogTouch (d, e.pressed); }
            else if (func == "jog")      { perf->jogMove (d, e.delta / 128.0); }      // 128 pasos = 1 vuelta
            else if (func == "tempoNudge") { engine.nudge (d, juce::jlimit (-0.3, 0.3, e.delta * 0.02)); }
            else if (func == "tempo")
            {
                // Abajo = más rápido (como un CDJ): 0 -> -rango, 1 -> +rango.
                player.setTempo (d, (float) ((e.value * 2.0 - 1.0) * controller.getTempoRange (d)));
            }
            else if (func == "tempoRange") { if (press) controller.cycleTempoRange (d); }
            else if (func == "load")
            {
                if (press)
                    if (const auto f = libraryView.selectedFile(); f.existsAsFile())
                        player.loadFile (d, f);
            }
            return;
        }
        if (head == "mixer")
        {
            if (parts.size() == 3)
            {
                const auto d = parts[1].getLastCharacters (1).getIntValue() - 1;
                if (! juce::isPositiveAndBelow (d, 2)) return;
                const auto v = (float) e.value;
                if (func == "volume")      engine.setChannelVolume (d, v);
                else if (func == "gain")   engine.setTrim (d, v);
                else if (func == "high")   engine.setEq (d, engine::EqBand::high, v);
                else if (func == "mid")    engine.setEq (d, engine::EqBand::mid, v);
                else if (func == "low")    engine.setEq (d, engine::EqBand::low, v);
                else if (func == "filter") engine.setFilter (d, v);
                else if (func == "cue" && press) engine.setChannelCue (d, ! engine.getChannelCue (d));
                return;
            }
            if (func == "crossfader")  engine.setCrossfader ((float) (e.value * 2.0 - 1.0));
            else if (func == "master") engine.setMasterVolume ((float) e.value);
            else if (func == "phones") engine.setHeadphoneVolume ((float) e.value);
            else if (func == "cueMix") engine.setCueMix ((float) e.value);
            else if (func == "mixTime")
            {
                autoMixSeconds = ui::ControllerView::knobToMixSeconds (e.value);
                settings.setAutoMixSeconds (autoMixSeconds);
                controller.setAutoMixSeconds (autoMixSeconds);
            }
            else if (func == "split" && press)   engine.setSplit (! engine.getSplit());
            else if (func == "autoMix" && press) toggleAutoMix();
            return;
        }
        if (head.startsWith ("fx"))
        {
            const auto u = head.getLastCharacters (1).getIntValue() - 1;
            if (! juce::isPositiveAndBelow (u, engine::AudioEngine::kNumFxUnits)) return;
            auto& unit = engine.getFxUnit (u);
            if (func == "on" && press)             unit.setOn (! unit.isOn());
            else if (func == "wet")                unit.setWet ((float) e.value);
            else if (func == "param")              unit.setParam ((float) e.value);
            else if (func == "beatsDown" && press) unit.setBeatsIndex (unit.getBeatsIndex() - 1);
            else if (func == "beatsUp" && press)   unit.setBeatsIndex (unit.getBeatsIndex() + 1);
            updateFxPanel();
            return;
        }
        if (head == "library")
        {
            if (func == "browse" && ! juce::exactlyEqual (e.delta, 0.0))
                libraryView.moveSelection (e.delta > 0.0 ? 1 : -1);
            else if (func == "largeView" && press)
                setLargeLibrary (! largeLibrary);
        }
    }

    bool MainComponent::midiLedState (const juce::String& id)
    {
        auto& engine = player.getEngine();
        const auto parts = juce::StringArray::fromTokens (id, ".", "");
        const auto& head = parts[0];
        const auto& func = parts[parts.size() - 1];
        if (head.startsWith ("deck"))
        {
            const auto d = head.getLastCharacters (1).getIntValue() - 1;
            if (! juce::isPositiveAndBelow (d, 2)) return false;
            const auto st = engine.getDeckStatus (d);
            const auto loaded = st.trackId != 0;
            if (func == "play" || func == "playPause") return st.playing && ! st.cuePreview;
            if (func == "pause")   return loaded && ! st.playing;
            if (func == "cue")     return loaded && (st.cuePreview || (! st.playing && std::abs (st.positionSeconds - st.cueSeconds) < 0.001));
            if (func == "sync")    return engine.getSync (d);
            if (func == "keyLock") return engine.getKeyLock (d);
            if (func == "keyDown") return engine.getKeyShift (d) < 0;
            if (func == "keyUp")   return engine.getKeyShift (d) > 0;
            if (func == "slip")    return performance->isSlip (d);
            if (func == "vinyl")   return performance->isVinyl (d);
            if (func == "shift")   return performance->isShift (d);
            if (func == "loopOut") return st.loopActive;
            if (func == "loopIn")  return st.loopActive;
            const auto pv = performance->makePadView (d);
            if (func == "modeHotCue")  return pv.group == 0;
            if (func == "modeFx")      return pv.group == 1;
            if (func == "modeSampler") return pv.group == 2;
            if (func == "modeLoop")    return pv.group == 3;
            if (func.startsWith ("pad")) return pv.lit[(size_t) juce::jlimit (0, 3, func.getLastCharacters (1).getIntValue() - 1)];
            return false;
        }
        if (head == "mixer")
        {
            if (parts.size() == 3 && func == "cue")
                return engine.getChannelCue (parts[1].getLastCharacters (1).getIntValue() - 1);
            if (func == "split")   return engine.getSplit();
            if (func == "autoMix") return engine.isAutoMixing();
            return false;
        }
        if (head.startsWith ("fx") && func == "on")
            return engine.getFxUnit (head.getLastCharacters (1).getIntValue() - 1).isOn();
        if (head == "library" && func == "largeView")
            return largeLibrary;
        return false;
    }

    void MainComponent::syncControlsFromEngine()
    {
        // Lo que se mueve desde un controlador MIDI se ve en pantalla.
        auto& engine = player.getEngine();
        for (int d = 0; d < 2; ++d)
        {
            controller.setVolume (d, engine.getChannelVolume (d));
            controller.setTrim (d, engine.getTrim (d));
            controller.setFilter (d, engine.getFilter (d));
            for (int band = 0; band < 3; ++band)
                controller.setEq (d, band, engine.getEq (d, (engine::EqBand) band));
            controller.setChannelCue (d, engine.getChannelCue (d));
        }
        controller.setMaster (engine.getMasterVolume());
        controller.setPhones (engine.getHeadphoneVolume());
        controller.setCueMix (engine.getCueMix());
    }

    void MainComponent::updateFxPanel()
    {
        for (int u = 0; u < engine::AudioEngine::kNumFxUnits; ++u)
        {
            const auto& unit = player.getEngine().getFxUnit (u);
            ui::FxPanel::UnitState st;
            st.type = (int) unit.getType();
            st.on = unit.isOn();
            st.wet = unit.getWet();
            st.param = unit.getParam();
            st.beatsIndex = unit.getBeatsIndex();
            st.target = (int) unit.getTarget();
            st.usesBeats = engine::fx::FxUnit::usesBeats (unit.getType());
            fxPanel.setUnit (u, st);
        }
    }

    void MainComponent::saveFx()
    {
        juce::Array<juce::var> list;
        for (int u = 0; u < engine::AudioEngine::kNumFxUnits; ++u)
        {
            const auto& unit = player.getEngine().getFxUnit (u);
            auto* o = new juce::DynamicObject();
            o->setProperty ("type", (int) unit.getType());
            o->setProperty ("beats", unit.getBeatsIndex());
            o->setProperty ("wet", unit.getWet());
            o->setProperty ("param", unit.getParam());
            o->setProperty ("target", (int) unit.getTarget());
            list.add (juce::var (o));
        }
        settings.setFxState (juce::JSON::toString (juce::var (list), true));
    }

    void MainComponent::restoreFx()
    {
        auto& engine = player.getEngine();
        // Por defecto: FX 1 = eco en el deck 1, FX 2 = filtro en el deck 2.
        engine.getFxUnit (0).setType (engine::fx::Type::echo);
        engine.getFxUnit (0).setTarget (engine::fx::Target::deck1);
        engine.getFxUnit (1).setType (engine::fx::Type::filter);
        engine.getFxUnit (1).setTarget (engine::fx::Target::deck2);

        const auto json = juce::JSON::parse (settings.getFxState());
        if (const auto* list = json.getArray())
        {
            for (int u = 0; u < juce::jmin (engine::AudioEngine::kNumFxUnits, list->size()); ++u)
            {
                const auto& v = (*list)[u];
                auto& unit = engine.getFxUnit (u);
                unit.setType ((engine::fx::Type) juce::jlimit (0, (int) engine::fx::Type::count - 1, (int) v.getProperty ("type", 0)));
                unit.setBeatsIndex ((int) v.getProperty ("beats", engine::fx::kDefaultBeatsIndex));
                unit.setWet ((float) (double) v.getProperty ("wet", 0.5));
                unit.setParam ((float) (double) v.getProperty ("param", 0.5));
                unit.setTarget ((engine::fx::Target) juce::jlimit (0, 2, (int) v.getProperty ("target", 2)));
            }
        }
    }

    void MainComponent::toggleAutoMix()
    {
        auto& engine = player.getEngine();
        if (engine.isAutoMixing())
        {
            engine.cancelAutoMix();
            return;
        }

        const auto source = engine::AudioEngine::autoMixSource (engine.getCrossfader(), engine.getCrossfaderReverse(),
                                                                engine.getDeckStatus (0).playing,
                                                                engine.getDeckStatus (1).playing);
        const auto target = 1 - source;
        if (player.getTrack (target) == nullptr)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::InfoIcon)
                                              .withTitle (tr ("automix.title"))
                                              .withMessage (tr ("automix.needTrack", { { "deck", juce::String (target + 1) } }))
                                              .withButton (tr ("common.ok"))
                                              .withAssociatedComponent (this),
                                          nullptr);
            return;
        }
        engine.startAutoMix (autoMixSeconds);
    }

    juce::String MainComponent::describeError (engine::LoadError error)
    {
        using E = engine::LoadError;
        switch (error)
        {
            case E::fileNotFound:      return tr ("load.error.fileNotFound");
            case E::unsupportedFormat: return tr ("load.error.unsupportedFormat",
                                                  { { "formats", engine::formats::supportedExtensions().joinIntoString (", ").toUpperCase() } });
            case E::unreadable:        return tr ("load.error.unreadable");
            case E::empty:             return tr ("load.error.empty");
            case E::tooLong:           return tr ("load.error.tooLong");
            case E::outOfMemory:       return tr ("load.error.outOfMemory");
            case E::none:
            case E::cancelled:         break;
        }
        return {};
    }

    // =========================================================================
    //  Arrastrar y soltar desde el Explorador
    // =========================================================================
    bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (const auto& f : files)
        {
            const juce::File file (f);
            if (engine::formats::isSupportedFile (file) || file.isDirectory())
                return true;
        }
        return false;
    }

    int MainComponent::chooseFreeDeck() const
    {
        const auto a = player.getStatus (0), b = player.getStatus (1);
        if (a.playing && ! b.playing) return 1;
        if (! a.playing && b.playing) return 0;
        if (player.getTrack (0) != nullptr && player.getTrack (1) == nullptr) return 1;
        return 0;
    }

    // --- Filas de la biblioteca arrastradas a un deck ------------------------------------
    bool MainComponent::isInterestedInDragSource (const SourceDetails& d)
    {
        return d.description.toString().startsWith (LibraryView::kDragPrefix);
    }

    void MainComponent::itemDragEnter (const SourceDetails& d) { itemDragMove (d); }
    void MainComponent::itemDragMove (const SourceDetails& d)
    {
        const auto deck = deckForDrop (d.localPosition.x, d.localPosition.y);
        setDropTarget (deck >= 0 ? deck : -1);
    }
    void MainComponent::itemDragExit (const SourceDetails&) { setDropTarget (-1); }

    void MainComponent::itemDropped (const SourceDetails& d)
    {
        const auto deck = deckForDrop (d.localPosition.x, d.localPosition.y);
        setDropTarget (-1);
        const auto file = LibraryView::fileFromDrag (d.description.toString());
        if (dropOnPad (d.localPosition.x, d.localPosition.y, file))
            return;
        if (deck >= 0)
            player.loadFile (deck, file);
    }

    bool MainComponent::dropOnPad (int x, int y, const juce::File& file)
    {
        if (! controllerArea().contains (x, y) || ! engine::formats::isSupportedFile (file))
            return false;
        int padIndex = -1;
        const auto d = controller.padAt (controller.getLocalPoint (this, juce::Point<int> (x, y)), padIndex);
        return d >= 0 && performance->dropOnPad (d, padIndex, file);
    }

    int MainComponent::deckForDrop (int x, int y) const
    {
        const juce::Point<int> p (x, y);

        for (int d = 0; d < 2; ++d)
            if (deckDisplays[d].getBounds().contains (p))
                return d;

        if (controllerArea().contains (p))
        {
            const auto local = controller.getLocalPoint (this, p);
            const auto d = controller.deckAt (local);
            if (d >= 0)
                return d;
            // Sobre el mixer: el lado más cercano.
            return local.x < controller.getWidth() / 2 ? 0 : 1;
        }

        if (libraryView.getBounds().contains (p))
            return kLibraryTarget;
        return -1;
    }

    void MainComponent::setDropTarget (int deck)
    {
        dropTarget = deck;
        controller.setDropTarget (deck);
        for (int d = 0; d < 2; ++d)
            deckDisplays[d].setDropHighlight (d == deck);
    }

    void MainComponent::fileDragEnter (const juce::StringArray&, int x, int y) { setDropTarget (juce::jmax (-1, deckForDrop (x, y))); }
    void MainComponent::fileDragMove (const juce::StringArray&, int x, int y)  { setDropTarget (juce::jmax (-1, deckForDrop (x, y))); }
    void MainComponent::fileDragExit (const juce::StringArray&)                { setDropTarget (-1); }

    void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
    {
        const auto deck = deckForDrop (x, y);
        setDropTarget (-1);

        if (deck == kLibraryTarget)
        {
            // Sobre la biblioteca: se agregan los archivos (y las carpetas, con todo su contenido).
            juce::Array<juce::File> toAdd;
            for (const auto& f : files)
            {
                const juce::File file (f);
                if (file.isDirectory())
                    library.addFolder (file);
                else
                    toAdd.add (file);
            }
            library.addFiles (toAdd);
            return;
        }
        if (deck < 0)
            return;

        // Se carga el primer archivo de audio válido (en un pad del sampler, o en el deck).
        for (const auto& f : files)
        {
            const juce::File file (f);
            if (dropOnPad (x, y, file))
                return;
            if (engine::formats::isSupportedFile (file))
            {
                player.loadFile (deck, file);
                return;
            }
        }
    }

    // =========================================================================
    //  Refresco de la UI
    // =========================================================================
    void MainComponent::timerCallback()
    {
        ++tick;
        checkLongPress();

        // Transición animada de la vista "Biblioteca grande" (~200 ms, con suavizado).
        const auto target = largeLibrary ? 1.0f : 0.0f;
        if (! juce::exactlyEqual (libraryMix, target))
        {
            const auto step = 1.0f / 6.0f;
            libraryMix = libraryMix < target ? juce::jmin (target, libraryMix + step) : juce::jmax (target, libraryMix - step);
            resized();
        }
        const bool blink = (tick / (kUiRefreshHz / 4)) % 2 == 0;   // ~2 Hz

        for (int d = 0; d < 2; ++d)
        {
            const auto st = player.getStatus (d);
            const auto track = player.getTrack (d);

            ui::ControllerView::DeckLights lights;
            lights.loaded     = track != nullptr && st.trackId == track->getId();
            lights.playing    = st.playing;
            lights.cuePreview = st.cuePreview;
            lights.atCue      = std::abs (st.positionSeconds - st.cueSeconds) < 0.001;
            controller.setDeckLights (d, lights, blink);

            auto& engine = player.getEngine();
            controller.setTempo (d, engine.getTempo (d));
            controller.setSyncLit (d, engine.getSync (d));
            controller.setKeyState (d, engine.getKeyLock (d), engine.getKeyShift (d));
            controller.setPadView (d, performance->makePadView (d));

            // Historial: la canción cuenta como tocada cuando empieza a sonar.
            if (st.playing && ! playedMarked[(size_t) d] && track != nullptr && st.trackId == track->getId())
            {
                playedMarked[(size_t) d] = true;
                library.markPlayed (track->getInfo().file);
            }
        }

        // Colores de compatibilidad armónica en la lista: respecto de lo que está sonando.
        std::vector<int> playingKeys;
        for (int d = 0; d < 2; ++d)
        {
            const auto v = makeDeckView (d);
            if (v.track != nullptr && v.status.playing && v.track->getKey() >= 0)
                playingKeys.push_back (analysis::MusicalKey::transpose (
                    v.track->getKey(), ui::DeckDisplay::effectiveShift (v.tempo, v.keyLock, v.keyShift)));
        }
        libraryView.setReferenceKeys (playingKeys);

        // MIDI: luces del controlador y controles en pantalla.
        midiController->updateLeds();
        syncControlsFromEngine();
        if (tick % 3 == 0)
            updateFxPanel();
        if (midiStatusTicks > 0 && --midiStatusTicks == 0)
        {
            midiStatus.clear();
            updateStatusBar();
        }

        // El AUTO MIX mueve el crossfader solo: la perilla lo sigue.
        controller.setCrossfader (player.getEngine().getCrossfader());
        controller.setAutoMixState (player.getEngine().getAutoMixProgress());

        // SPLIT también se cambia desde Opciones: reflejarlo en el botón.
        controller.setSplit (player.getEngine().getSplit());

        if (tick % (kUiRefreshHz / 4) == 0)
            updateStatusBar();
        if (tick % kUiRefreshHz == 0)
            updateRenderer();   // la opción de GPU se cambia desde Preferencias
    }

    ui::DeckDisplay::View MainComponent::makeDeckView (int d) const
    {
        auto& engine = player.getEngine();
        ui::DeckDisplay::View v;
        v.track     = player.getTrack (d);
        v.status    = engine.getDeckStatus (d);
        v.loading   = player.isLoading (d);
        v.analysing = player.isAnalysing (d);
        v.tempo     = engine.getTempo (d);
        v.keyLock   = engine.getKeyLock (d);
        v.keyShift  = engine.getKeyShift (d);
        v.artwork   = deckArtwork[(size_t) d];
        if (deckTags[(size_t) d].valid)
        {
            v.title  = deckTags[(size_t) d].title;
            v.artist = deckTags[(size_t) d].artist;
        }
        return v;
    }

    void MainComponent::updateMeters()
    {
        const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const auto dt  = lastMeterTime > 0.0 ? (float) juce::jlimit (0.0, 0.1, now - lastMeterTime) : 1.0f / 60.0f;
        lastMeterTime = now;

        auto& engine = player.getEngine();
        for (int d = 0; d < 2; ++d)
        {
            const auto st = engine.getDeckStatus (d);
            if (const auto t = player.getTrack (d))
                controller.setPlatterBeatGrid (d, t->getBpm(), t->getFirstBeatSeconds());   // en tiempo de la pista
            controller.setPlatter (d, st.positionSeconds, st.playing, st.trackId != 0);
            deckDisplays[d].update (makeDeckView (d));   // waveform ampliada a 60 fps
        }
        controller.pushChannelMeter (0, engine.readMeter (engine::MeterId::channel1), dt);
        controller.pushChannelMeter (1, engine.readMeter (engine::MeterId::channel2), dt);
        topBar.pushMaster (engine.readMeter (engine::MeterId::masterLeft),
                           engine.readMeter (engine::MeterId::masterRight),
                           engine.getMasterLimiterReductionDb(), dt);
    }

    void MainComponent::languageChanged()
    {
        updateStatusBar();
        controller.invalidateBackground();
        fxPanel.refreshTexts();
        // Los componentes traducen al pintar: alcanza con repintar.
        for (auto* c : getChildren())
            c->repaint();
        for (auto* c : controller.getChildren())
            c->repaint();
    }

    void MainComponent::updateStatusBar()
    {
        const auto st = audio.getStatus();
        juce::String left;

        if (st.open)
        {
            left = tr ("audio.status.running", {
                { "type",    st.typeName },
                { "device",  st.deviceName },
                { "rate",    juce::String (juce::roundToInt (st.sampleRate)) },
                { "buffer",  juce::String (st.bufferSize) },
                { "latency", juce::String (st.outputLatencyMs, 1) },
                { "outputs", juce::String (st.numOutputs) },
                { "cpu",     juce::String (st.cpuPercent, 1) } });
        }
        else
        {
            left = tr ("audio.status.noDevice");
        }

        // MIDI Learn: el pie avisa qué control se está esperando.
        if (midiController != nullptr && midiController->isLearning())
            left = tr ("midi.learning", { { "name", midiTargetName (midiController->getLearnTarget()) } });
        else if (midiStatus.isNotEmpty())
            left = midiStatus;
        else if (midiController != nullptr && ! midiController->getConnectedDevices().isEmpty())
            left << "  ·  MIDI: " << midiController->getConnectedDevices().joinIntoString (", ");

        statusBar.setText (left, "v" + juce::JUCEApplication::getInstance()->getApplicationVersion());
    }
} // namespace kb::app
