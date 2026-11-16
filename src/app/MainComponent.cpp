#include "app/MainComponent.h"

#include "app/AudioDeviceService.h"
#include "engine/AudioFileFormats.h"
#include "analysis/KeyDetector.h"
#include "ui/Palette.h"

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
        controller.onPadMenu  = [perf] (int d, int i, juce::Component* t) { perf->padMenu (d, i, t); };
        controller.onShift    = [perf] (int d, bool down) { perf->shift (d, down); };
        controller.onSlip     = [perf] (int d) { perf->toggleSlip (d); };
        controller.onVinyl    = [perf] (int d) { perf->toggleVinyl (d); };
        controller.onJogTouch = [perf] (int d, bool t) { perf->jogTouch (d, t); };
        controller.onJogTurn  = [perf] (int d, double turns) { perf->jogMove (d, turns); };
        controller.onJogWheel = [perf] (int d, float w) { perf->jogWheel (d, w); };

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
        juce::ColourGradient grad (chassisLight, 0.0f, 0.0f, chassisShadow, 0.0f, b.getBottom(), false);
        grad.addColour (0.18, chassis);
        grad.addColour (0.8, chassis.darker (0.08f));
        g.setGradientFill (grad);
        g.fillPath (body);
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

    void MainComponent::resized()
    {
        // Layout de la imagen de referencia: barra superior amarilla, controlador,
        // pantalla oscura con las dos waveforms lado a lado y la biblioteca, y pie.
        frameImage = {};
        const auto H = getHeight();
        const auto m = frameThickness();
        auto b = getLocalBounds().reduced (m + m / 2, m / 2);

        statusBar.setBounds (b.removeFromBottom (juce::jlimit (20, 40, juce::roundToInt (H * 0.034))));
        topBar.setBounds     (b.removeFromTop (juce::roundToInt (H * 0.075)));
        controller.setBounds (b.removeFromTop (juce::roundToInt (H * 0.465)));
        b.removeFromTop (m / 2);
        b.removeFromBottom (m / 3);
        screenArea = b;

        auto inner = b.reduced (m / 2 + 2);
        auto waves = inner.removeFromTop (juce::roundToInt (inner.getHeight() * 0.42));
        const auto gap = m / 2;
        deckDisplays[0].setBounds (waves.removeFromLeft ((waves.getWidth() - gap) / 2));
        waves.removeFromLeft (gap);
        deckDisplays[1].setBounds (waves);
        inner.removeFromTop (m / 3);
        fxPanel.setBounds (inner.removeFromTop (juce::jlimit (34, 60, juce::roundToInt (H * 0.045))));
        inner.removeFromTop (m / 3);
        libraryView.setBounds (inner);
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
                                  if (safe != nullptr && fc.getResult().existsAsFile())
                                      safe->player.loadFile (deck, fc.getResult());
                              });
    }

    void MainComponent::trackLoaded (int deck)
    {
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
        const juce::File file (d.description.toString().fromFirstOccurrenceOf (LibraryView::kDragPrefix, false, false));
        if (dropOnPad (d.localPosition.x, d.localPosition.y, file))
            return;
        if (deck >= 0)
            player.loadFile (deck, file);
    }

    bool MainComponent::dropOnPad (int x, int y, const juce::File& file)
    {
        if (! controller.getBounds().contains (x, y) || ! engine::formats::isSupportedFile (file))
            return false;
        int padIndex = -1;
        const auto d = controller.padAt (juce::Point<int> (x, y) - controller.getPosition(), padIndex);
        return d >= 0 && performance->dropOnPad (d, padIndex, file);
    }

    int MainComponent::deckForDrop (int x, int y) const
    {
        const juce::Point<int> p (x, y);

        for (int d = 0; d < 2; ++d)
            if (deckDisplays[d].getBounds().contains (p))
                return d;

        if (controller.getBounds().contains (p))
        {
            const auto local = p - controller.getPosition();
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

        // El AUTO MIX mueve el crossfader solo: la perilla lo sigue.
        controller.setCrossfader (player.getEngine().getCrossfader());
        controller.setAutoMixState (player.getEngine().getAutoMixProgress());

        // SPLIT también se cambia desde Opciones: reflejarlo en el botón.
        controller.setSplit (player.getEngine().getSplit());

        if (tick % (kUiRefreshHz / 4) == 0)
            updateStatusBar();
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

        statusBar.setText (left, "v" + juce::JUCEApplication::getInstance()->getApplicationVersion());
    }
} // namespace kb::app
